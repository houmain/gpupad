#pragma once

#include <kiss_fftr.h>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <vector>

// based on https://github.com/kylemcdonald/ofxFft
class KissFFT
{
private:
    const int m_signal_size;
    const int m_bin_size;
    kiss_fftr_cfg m_kiss_fftr{ };
    std::vector<float> m_window;
    std::vector<float> m_windowed_signal;
    float m_two_over_window_sum{ };
    std::vector<kiss_fft_cpx> m_cx_out;
    std::vector<float> m_amplitudes;

public:
    enum class WindowType {
        rectangular,
        bartlett,
        hann,
        hamming,
        sine,
    };

    KissFFT(int signal_size, WindowType window_type)
        : m_signal_size(signal_size)
        , m_bin_size(signal_size / 2 + 1)
    {
        constexpr auto Pi = std::numbers::pi_v<double>;

        m_kiss_fftr = kiss_fftr_alloc(signal_size, 0, nullptr, nullptr);
        if (!m_kiss_fftr)
            throw std::runtime_error("initializing kiss fft failed");

        m_window.resize(m_signal_size, 1.0f);
        m_windowed_signal.resize(m_signal_size);
        m_cx_out.resize(m_bin_size);
        m_amplitudes.resize(m_bin_size);

        switch (window_type) {
        case WindowType::rectangular: break;
        case WindowType::bartlett:    {
            const auto half = m_signal_size / 2;
            for (auto i = 0; i < half; ++i) {
                m_window[i] = static_cast<float>(i) / half;
                m_window[i + half] = 1.0f - (static_cast<float>(i) / half);
            }
            break;
        }
        case WindowType::hann:
            for (auto i = 0; i < m_signal_size; ++i)
                m_window[i] = static_cast<float>(
                    0.5 * (1 - std::cos((2 * Pi * i) / (m_signal_size - 1))));
            break;
        case WindowType::hamming:
            for (auto i = 0; i < m_signal_size; ++i)
                m_window[i] = static_cast<float>(
                    0.54 - 0.46 * std::cos((2 * Pi * i) / (m_signal_size - 1)));
            break;
        case WindowType::sine:
            for (auto i = 0; i < m_signal_size; ++i)
                m_window[i] = static_cast<float>(
                    std::sin((Pi * i) / (m_signal_size - 1)));
            break;
        }

        m_two_over_window_sum = 2.0f
            / std::accumulate(m_window.begin(), m_window.end(), 0.0f);
    }

    KissFFT(const KissFFT &) = delete;
    KissFFT &operator=(const KissFFT &) = delete;

    ~KissFFT() { kiss_fftr_free(m_kiss_fftr); }

    void set_signal(const float *signal)
    {
        // apply window
        for (auto i = 0; i < m_signal_size; ++i)
            m_windowed_signal[i] = signal[i] * m_window[i];

        // execute fft
        kiss_fftr(m_kiss_fftr, m_windowed_signal.data(), m_cx_out.data());

        for (auto i = 0; i < m_bin_size; ++i) {
            auto [real, imag] = m_cx_out[i];
            real *= m_two_over_window_sum;
            imag *= m_two_over_window_sum;
            m_amplitudes[i] = cartesian_to_amplitude(real, imag);
        }
    }

    int signal_size() const { return m_signal_size; }
    int bin_size() const { return m_bin_size; }
    const std::vector<float> &amplitudes() const { return m_amplitudes; }

private:
    static float cartesian_to_amplitude(float x, float y)
    {
        return std::sqrt(x * x + y * y);
    }
    static float cartesian_to_phase(float x, float y)
    {
        return std::atan2(y, x);
    }
};
