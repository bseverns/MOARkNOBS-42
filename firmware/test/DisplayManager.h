#pragma once

// Bare-bones DisplayManager stand-in so tests can link without the real UI stack.
class DisplayManager {
public:
    void registerInteraction();
};

