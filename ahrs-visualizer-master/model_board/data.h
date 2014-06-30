#define EX 20     // X dimension
#define EY 20     // Y dimension
#define EZ 0.6   // Thickness
#define AL 10    // axes length
#define H(rgb) (rgb >> 16 & 0xFF)/255., (rgb >> 8 & 0xFF)/255., (rgb & 0xFF)/255.
// Inverted 'Z' order:
#define CTOP1 H(0x008d3a)
#define CTOP2 H(0x007b34)
#define CTOP3 H(0x0ba350)
#define CTOP4 H(0x00983c)

#define CBOT1 H(0x008b4c)
#define CBOT2 H(0x009059)
#define CBOT3 H(0x04a762)
#define CBOT4 H(0x029d67)




static const GLfloat colors[] = {
    CTOP1,  1,
    CTOP2,  1,
    CTOP3,  1,
    CTOP4,  1,

    CBOT1,  1,
    CBOT2,  1,
    CBOT3,  1,
    CBOT4,  1,

    CBOT4,  1,
    CBOT2,  1,
    CTOP3,  1,
    CTOP1,  1,

    CBOT1,  1,
    CBOT3,  1,
    CTOP2,  1,
    CTOP4,  1,

    CBOT3,  1,
    CBOT4,  1,
    CTOP4,  1,
    CTOP3,  1,

    CBOT2, 1,
    CBOT1, 1,
    CTOP1, 1,
    CTOP2, 1,

    // X axis: red
    1, 0, 0, 1,
    1, 0, 0, 1,

    // Y axis: green
    0, 1, 0, 1,
    0, 1, 0, 1,
   
    // Z axis: blue
    0, 0, 1, 1,
    0, 0, 1, 1,

    // Acceleration: cyan
    0, 1, 1, 1,
    0, 1, 1, 1,

    // Magnetic field: yellow
    1, 1, 0, 1,
    1, 1, 0, 1,
};

