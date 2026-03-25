#include "app.h"

#include <exception>

int main()
{
    try
    {
        App app;
        app.Run();
    }
    catch (const std::exception& exception)
    {
        SDL_Log("Fatal error: %s", exception.what());
        return 1;
    }

    return 0;
}
