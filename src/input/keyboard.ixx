module;

export module stellar.input.keyboard;

export enum class Key;
export enum class KeyState;

export struct KeyboardEvent {
    Key key;
    KeyState state;
};

enum class KeyState {
    Pressed,
    Released
};

enum class Key {
    Unknown,
    KeyA,
    KeyD,
    KeyS,
    KeyW
};