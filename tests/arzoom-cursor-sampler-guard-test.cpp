#include "arzoom-cursor-sampler-guard.hpp"

#include <cassert>
#include <iostream>

int main()
{
    using arzoom::CursorSamplerRoute;
    using arzoom::cursor_sampler_route;

    assert(cursor_sampler_route(false, false, true, false) ==
           CursorSamplerRoute::TransparentFallback);
    assert(cursor_sampler_route(true, false, true, true) ==
           CursorSamplerRoute::TransparentFallback);
    assert(cursor_sampler_route(true, true, false, true) ==
           CursorSamplerRoute::TransparentFallback);
    assert(cursor_sampler_route(true, true, true, false) ==
           CursorSamplerRoute::TransparentFallback);
    assert(cursor_sampler_route(true, true, true, true) ==
           CursorSamplerRoute::RealAtlas);

    std::cout << "Cursor sampler prebind routing invariants passed\n";
    return 0;
}
