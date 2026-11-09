#include "TailConvolver.h"

namespace lucky
{

namespace
{
constexpr int fftOrder = 13;
constexpr int fftSize = 1 << fftOrder;
constexpr int bins = fftSize / 2 + 1;
static_assert (fftSize == 2 * TailConvolver::partitionSize);

/** acc += a * b over interleaved complex spectra. */
void multiplyAccumulate (float* acc, const float* a, const float* b) noexcept
{
    for (int k = 0; k < bins; ++k)
    {
        const auto ar = a[2 * k], ai = a[2 * k + 1], br = b[2 * k], bi = b[2 * k + 1];
        acc[2 * k] += ar * br - ai * bi;
        acc[2 * k + 1] += ar * bi + ai * br;
    }
}
} // namespace

struct TailConvolver::Spectra
{
    std::array<std::vector<std::vector<float>>, 2> partitions;
    int length = 0;
};

TailConvolver::TailConvolver() : fft (fftOrder)
{
    frame.assign (2 * fftSize, 0.0f);
    previousBlock.assign (partitionSize, 0.0f);
    inputBlock.assign (partitionSize, 0.0f);
    for (auto* channels : { &accumulator, &previousAccumulator })
        for (auto& channel : *channels)
            channel.assign (2 * fftSize, 0.0f);
}

TailConvolver::~TailConvolver()
{
    delete pending.exchange (nullptr);
}

void TailConvolver::prepare (int maximumBlockSize)
{
    const auto capacity = headLength + 2 * partitionSize + 4 * std::max (maximumBlockSize, 1);
    inputFifo.setTotalSize (capacity);
    outputFifo.setTotalSize (capacity);
    inputRing.assign (static_cast<size_t> (capacity), 0.0f);
    for (auto& channel : outputRing)
        channel.assign (static_cast<size_t> (capacity), 0.0f);
    reset();
}

void TailConvolver::reset()
{
    const juce::SpinLock::ScopedLockType scope (processLock);
    inputFifo.reset();
    outputFifo.reset();
    std::fill (previousBlock.begin(), previousBlock.end(), 0.0f);
    for (auto& spectrum : history)
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
    skipDebt = 0;

    const auto scope2 = outputFifo.write (headLength);
    for (auto& channel : outputRing)
    {
        std::fill_n (channel.begin() + scope2.startIndex1, scope2.blockSize1, 0.0f);
        std::fill_n (channel.begin() + scope2.startIndex2, scope2.blockSize2, 0.0f);
    }
}

std::unique_ptr<TailConvolver::Spectra> TailConvolver::makeSpectra (const juce::AudioBuffer<float>& tail)
{
    auto spectra = std::make_unique<Spectra>();
    spectra->length = tail.getNumSamples();
    if (spectra->length == 0)
        return spectra;

    juce::dsp::FFT transform (fftOrder);
    const auto count = (spectra->length + partitionSize - 1) / partitionSize;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* source = tail.getReadPointer (std::min (ch, tail.getNumChannels() - 1));
        for (int p = 0; p < count; ++p)
        {
            std::vector<float> spectrum (2 * fftSize, 0.0f);
            const auto start = p * partitionSize;
            std::copy_n (source + start, std::min (partitionSize, spectra->length - start), spectrum.begin());
            transform.performRealOnlyForwardTransform (spectrum.data(), true);
            spectra->partitions[static_cast<size_t> (ch)].push_back (std::move (spectrum));
        }
    }
    return spectra;
}

void TailConvolver::setTail (const juce::AudioBuffer<float>& stereoTail)
{
    delete pending.exchange (makeSpectra (stereoTail).release());
}

void TailConvolver::ensureHistory (size_t partitions)
{
    if (history.size() >= partitions)
        return;

    std::vector<std::vector<float>> grown;
    grown.reserve (partitions);
    for (size_t i = 0; i < history.size(); ++i)
        grown.push_back (std::move (history[(historyFront + i) % history.size()]));
    while (grown.size() < partitions)
        grown.emplace_back (2 * fftSize, 0.0f);

    history = std::move (grown);
    historyFront = 0;
}

void TailConvolver::computeBlock()
{
    if (auto* incoming = pending.exchange (nullptr))
    {
        fading = std::move (active);
        active.reset (incoming);
        activeTailLength = active->length;
        ensureHistory (active->partitions[0].size());
    }

    std::copy (previousBlock.begin(), previousBlock.end(), frame.begin());
    std::copy (inputBlock.begin(), inputBlock.end(), frame.begin() + partitionSize);
    std::fill (frame.begin() + fftSize, frame.end(), 0.0f);
    std::copy (inputBlock.begin(), inputBlock.end(), previousBlock.begin());

    if (! history.empty())
    {
        historyFront = (historyFront + history.size() - 1) % history.size();
        auto& newest = history[historyFront];
        std::copy (frame.begin(), frame.end(), newest.begin());
        fft.performRealOnlyForwardTransform (newest.data(), true);
    }

    const auto convolve = [this] (const Spectra* spectra, std::array<std::vector<float>, 2>& out)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& acc = out[static_cast<size_t> (ch)];
            std::fill (acc.begin(), acc.end(), 0.0f);
            if (spectra == nullptr || spectra->length == 0)
                continue;

            const auto& parts = spectra->partitions[static_cast<size_t> (ch)];
            for (size_t p = 0; p < parts.size(); ++p)
                multiplyAccumulate (acc.data(), history[(historyFront + p) % history.size()].data(), parts[p].data());

            fft.performRealOnlyInverseTransform (acc.data());
        }
    };

    convolve (active.get(), accumulator);
    if (fading != nullptr)
        convolve (fading.get(), previousAccumulator);

    const auto writeScope = outputFifo.write (partitionSize);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* current = accumulator[static_cast<size_t> (ch)].data() + partitionSize;
        const auto* old = previousAccumulator[static_cast<size_t> (ch)].data() + partitionSize;
        auto* ring = outputRing[static_cast<size_t> (ch)].data();

        for (int i = 0; i < partitionSize; ++i)
        {
            auto value = current[i];
            if (fading != nullptr)
            {
                const auto fade = static_cast<float> (i) / static_cast<float> (partitionSize);
                value = old[i] * (1.0f - fade) + value * fade;
            }

            const auto index = i < writeScope.blockSize1 ? writeScope.startIndex1 + i : writeScope.startIndex2 + (i - writeScope.blockSize1);
            ring[index] = value;
        }
    }

    fading.reset();
}

void TailConvolver::processPending()
{
    const juce::SpinLock::ScopedTryLockType scope (processLock);
    if (! scope.isLocked())
        return;

    while (inputFifo.getNumReady() >= partitionSize && outputFifo.getFreeSpace() >= partitionSize)
    {
        const auto readScope = inputFifo.read (partitionSize);
        std::copy_n (inputRing.begin() + readScope.startIndex1, readScope.blockSize1, inputBlock.begin());
        std::copy_n (inputRing.begin() + readScope.startIndex2, readScope.blockSize2, inputBlock.begin() + readScope.blockSize1);
        computeBlock();
    }
}

void TailConvolver::process (const float* input, float* outL, float* outR, int n, bool realtime)
{
    const auto toWrite = std::min (n, inputFifo.getFreeSpace());
    const auto writeScope = inputFifo.write (toWrite);
    std::copy_n (input, writeScope.blockSize1, inputRing.begin() + writeScope.startIndex1);
    std::copy_n (input + writeScope.blockSize1, writeScope.blockSize2, inputRing.begin() + writeScope.startIndex2);

    if (! realtime)
    {
        const juce::SpinLock::ScopedLockType scope (processLock);
        while (inputFifo.getNumReady() >= partitionSize && outputFifo.getFreeSpace() >= partitionSize)
        {
            const auto readScope = inputFifo.read (partitionSize);
            std::copy_n (inputRing.begin() + readScope.startIndex1, readScope.blockSize1, inputBlock.begin());
            std::copy_n (inputRing.begin() + readScope.startIndex2, readScope.blockSize2, inputBlock.begin() + readScope.blockSize1);
            computeBlock();
        }
    }

    if (skipDebt > 0)
    {
        const auto skip = static_cast<int> (std::min<juce::int64> (skipDebt, outputFifo.getNumReady()));
        outputFifo.finishedRead (skip);
        skipDebt -= skip;
    }

    const auto available = std::min (n, outputFifo.getNumReady());
    const auto readScope = outputFifo.read (available);
    const auto addRange = [&] (int ringStart, int count, int outOffset)
    {
        juce::FloatVectorOperations::add (outL + outOffset, outputRing[0].data() + ringStart, count);
        juce::FloatVectorOperations::add (outR + outOffset, outputRing[1].data() + ringStart, count);
    };
    addRange (readScope.startIndex1, readScope.blockSize1, 0);
    addRange (readScope.startIndex2, readScope.blockSize2, readScope.blockSize1);

    if (available < n)
    {
        skipDebt += n - available;
        underflowSamples += n - available;
    }
}

} // namespace lucky
