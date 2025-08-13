#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

// Bare-bones DisplayManager stand-in so tests can link without the real UI stack.
class DisplayManager {
public:
    void registerInteraction();
};

#endif // DISPLAYMANAGER_H
