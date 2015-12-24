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
        Color,
        Luminosity,
        DestinationOut,
        SourceAtop
    } Mode;
}

#endif // BLENDMODES_H
