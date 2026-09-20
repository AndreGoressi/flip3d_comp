# Flip3DComp

<img width="3840" height="2160" alt="bFcAzo5fav" src="https://github.com/user-attachments/assets/02adc3b7-e1c4-4126-b65b-a1789353c0d5" />
<img width="3840" height="2160" alt="8HXYMtHEHN" src="https://github.com/user-attachments/assets/8a6ebf11-baed-45b6-b228-b95713c596bf" />
<img width="3840" height="2160" alt="kiF6r0nBkX" src="https://github.com/user-attachments/assets/757f0c14-9629-433d-b26e-e7bd1d87c6d2" />
<img width="3840" height="2160" alt="jk9bbkDuu0" src="https://github.com/user-attachments/assets/bad41c63-c440-49ef-a5bd-165ce9085b08" />
<img width="3840" height="2160" alt="XHXUfLaXKg" src="https://github.com/user-attachments/assets/f106beb3-5a29-4e3c-a08e-98ed170dad68" />

DirectComposition Flip3D switcher. A faster, more complete successor to the [flip3d](https://github.com/ALTaleX531/flip3d) D3D11 prototype.

## Requirements

- Windows with DWM and DirectComposition
- CMake 3.21+
- Visual Studio / MSVC with the Windows SDK

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

Launch `build/Release/Flip3DComp.exe`.

Eligible windows are shown as DWM shared thumbnails on a DirectComposition 3D carousel. No D3D11 scene pass, no Windows.Graphics.Capture.

## Controls

- `Tab` / `Shift+Tab`, arrow keys, mouse wheel: scroll the carousel
- `Enter` or left click a card: activate the selected window
- `Home`: return to the original front window
- `Esc`: exit

## Notes

- **vs [flip3d](https://github.com/ALTaleX531/flip3d):** compositor-native visuals (lower overhead), smooth fractional scroll while browsing, uDWM-aligned parallel exit rotation
- **More complete:** `IAccessible` + `NotifyWinEvent` accessibility, Shell Hook live card add/remove
