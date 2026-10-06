#pragma once

#include <array>
#include <string_view>

namespace returnline::art {

// Hand-drawn 1x pixel sprites. A dot is transparent; colors are resolved by the renderer.
inline constexpr std::array<std::string_view, 16> Ballast = {
    "................", "....s...........", "................", "..t......s......",
    "................", ".......l........", "................", "s...............",
    "................", "..........t.....", "................", "...c............",
    "................", ".............s..", "................", ".l.............."
};

inline constexpr std::array<std::string_view, 16> StationBrick = {
    "bbbbbbbbbbbbbbbb", "b..............b", "b..............b", "bbbb..bb..bbbbbbb",
    "b..............b", "b..............b", "bbbbbbbbbbbbbbbb", "b...bbbb........",
    "b..............b", "b..............b", "bbbbbbbbbbbbbbbb", "b..............b",
    "b..............b", "bbbb...bbbbbbbbb", "b..............b", "bbbbbbbbbbbbbbbb"
};

inline constexpr std::array<std::string_view, 24> Survivor = {
    ".......hhhh......", "......hhhhhh.....", ".....hhhhhhhh....", ".....hssssssh....",
    ".....ssssssss....", ".....ss.o.sss....", ".....ssssssss....", "......ssssss.....",
    ".....ooosssoo....", "....occccccco....", "...occcccccccco..", "..occcccccccccco.",
    "..occcggccccccco.", "..occcccccccccco.", "..occcccccccccco.", "...occcccccccco..",
    "...occcccccccco..", "...opppppppppo...", "...opppppppppo...", "...opppppppppo...",
    "...opppppppppo...", "....oppp..pppo...", "...obbb..bbbbo...", "...obbb..bbbbo..."
};

inline constexpr std::array<std::string_view, 22> Mutant = {
    "..........oooo..........", "........ooOOOOoo........", "......oooOOOOOOooo......",
    ".....ooOooOOOOooOoo.....", "....ooOOOooooooOOOoo....", "....oOOo..oo..oOOo.......",
    "...oOOOooooooOOOoo.......", "...oOOOOOOOOOOOOOOo......", "....oOOOOoOOoOOOOo........",
    "....oOOOOOOOOOOOOo........", ".....ooOOOOOOOOoo..........", "......oOOOOOOOOo...........",
    "...oooOOOOOOOOOOooo........", "..ooOooOOOOOOOOooOoo.......", ".ooOOo..OOOOOO..oOOoo......",
    "ooOOo...OOOOOO...oOOoo.....", "..oo....oOOOOo....oo.......", "........oO..Oo..............",
    ".......ooO..Ooo.............", "......oo......oo............", ".....oo........oo...........",
    "....oo..........oo.........."
};

inline constexpr std::array<std::string_view, 14> ScrapCrate = {
    ".....................", "....ooooooooooooo....", "...ohhhhhhhhhhhhho...",
    "..ohhhhhhhhhhhhhhhho..", "..ohggghhggghhggghho..", "..ohggghhggghhggghho..",
    "..ohhhhhhhhhhhhhhhho..", "..ooooooooooooooooooo..", "..occchhccchhccchhcco..",
    "..occchhccchhccchhcco..", "..occchhccchhccchhcco..", "...occccccccccccccco...",
    "....oooooooooooooooo...", "....................."
};

inline constexpr std::array<std::string_view, 14> FuelCans = {
    "......................", ".....oooo....oooo.....", "....ohhhho..ohhhho....",
    "....occcco..occcco....", "...occcccooocccccco...", "...occcccccgcccccco...",
    "...occhhccccchhcco....", "...occcccccccccccco...", "...occcccgccccccccco..",
    "...occcccccccccccco...", "...occhhccccchhcco....", "...occcccccgcccccco...",
    "....oooooooo..oooo....", "......................"
};

inline constexpr std::array<std::string_view, 10> MedKit = {
    "............", "...oooooooo.", "..ohhhhhhhho.", ".occcccccccco",
    ".occcccgccccco", ".occccgcccccco", ".occcccgccccco", ".occccccccccco",
    "..ooooooooooo.", "............"
};

} // namespace returnline::art
