#pragma once

struct DoubleRange 
{
    double minimum;
    double maximum;

    double range() const { return maximum - minimum; }

    friend bool operator==(const DoubleRange &a, const DoubleRange &b) 
    {
        return (a.minimum == b.minimum && a.maximum == b.maximum);
    }

    friend bool operator!=(const DoubleRange &a, const DoubleRange &b) 
    {
        return !(a == b);
    }
};
