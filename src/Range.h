#pragma once

struct Range 
{
    double minimum;
    double maximum;

    double range() const { return maximum - minimum; }

    friend bool operator==(const Range &a, const Range &b) 
    {
        return (a.minimum == b.minimum && a.maximum == b.maximum);
    }

    friend bool operator!=(const Range &a, const Range &b) 
    {
        return !(a == b);
    }
};
