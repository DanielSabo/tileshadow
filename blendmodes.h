#ifndef BLENDMODES_H
#define BLENDMODES_H

namespace BlendMode
{
    typedef enum {
        Over,
        Multiply,
        ColorDodge,
        ColorBurn,
        Screen,
        Hue,
        Saturation,
        Color,
        Luminosity,
        DestinationIn,
        DestinationOut,
        SourceAtop,
        DestinationAtop
    } Mode;

    static inline bool isMasking(Mode mode) {
        return (mode == DestinationIn || mode == DestinationAtop);
    }
}

#endif // BLENDMODES_H
