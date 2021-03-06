/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2016 - ROLI Ltd.

   Permission is granted to use this software under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license/

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
   OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
   USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
   TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
   OF THIS SOFTWARE.

   -----------------------------------------------------------------------------

   To release a closed-source product which uses other parts of JUCE not
   licensed under the ISC terms, commercial licenses are available: visit
   www.juce.com for more information.

  ==============================================================================
*/

#pragma once


//==============================================================================
/**
    This class contains some helpful static methods for dealing with decibel values.
*/
class Decibels
{
public:
    //==============================================================================
    /** Converts a dBFS value to its equivalent gain level.

        A gain of 1.0 = 0 dB, and lower gains map onto negative decibel values. Any
        decibel value lower than minusInfinityDb will return a gain of 0.
    */
    template <typename Type>
    static Type decibelsToGain (const Type decibels,
                                const Type minusInfinityDb = (Type) defaultMinusInfinitydB)
    {
        return decibels > minusInfinityDb ? std::pow ((Type) 10.0, decibels * (Type) 0.05)
                                          : Type();
    }

    /** Converts a gain level into a dBFS value.

        A gain of 1.0 = 0 dB, and lower gains map onto negative decibel values.
        If the gain is 0 (or negative), then the method will return the value
        provided as minusInfinityDb.
    */
    template <typename Type>
    static Type gainToDecibels (const Type gain,
                                const Type minusInfinityDb = (Type) defaultMinusInfinitydB)
    {
        return gain > Type() ? jmax (minusInfinityDb, (Type) std::log10 (gain) * (Type) 20.0)
                             : minusInfinityDb;
    }

    //==============================================================================
    /** Converts a decibel reading to a string, with the 'dB' suffix.
        If the decibel value is lower than minusInfinityDb, the return value will
        be "-INF dB".
    */
    template <typename Type>
    static String toString (const Type decibels,
                            const int decimalPlaces = 2,
                            const Type minusInfinityDb = (Type) defaultMinusInfinitydB)
    {
        String s;

        if (decibels <= minusInfinityDb)
        {
            s = "-INF dB";
        }
        else
        {
            if (decibels >= Type())
                s << '+';

            s << String (decibels, decimalPlaces) << " dB";
        }

        return s;
    }


private:
    //==============================================================================
    enum
    {
        defaultMinusInfinitydB = -100
    };

    Decibels(); // This class can't be instantiated, it's just a holder for static methods..
    JUCE_DECLARE_NON_COPYABLE (Decibels)
};
