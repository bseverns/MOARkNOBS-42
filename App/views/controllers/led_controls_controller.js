export function createLedControlsController({ container, runtime } = {}) {
  const state = {
    mounted: false,
    brightnessInput: null,
    brightnessValue: null,
    colorInput: null,
    colorValue: null
  };

  function render(staged) {
    if (!container) return;
    const led = staged?.led ?? { brightness: 0, color: '#000000' };
    const initialBrightness = Number.isFinite(Number(led.brightness)) ? Number(led.brightness) : 0;
    const initialColor =
      typeof led.color === 'string' && /^#([0-9a-fA-F]{6})$/.test(led.color)
        ? led.color
        : '#000000';

    if (!state.mounted) {
      container.innerHTML = '';

      const brightnessWrap = document.createElement('label');
      brightnessWrap.className = 'led-control';
      brightnessWrap.textContent = 'Brightness';
      const brightnessInput = document.createElement('input');
      brightnessInput.type = 'range';
      brightnessInput.min = '0';
      brightnessInput.max = '255';
      const brightnessValue = document.createElement('span');
      brightnessValue.className = 'led-value';
      const pushBrightness = runtime.createThrottle((value) => {
        brightnessValue.textContent = String(value);
        runtime.stage((draft) => {
          draft.led = draft.led || { brightness: 0, color: '#000000' };
          draft.led.brightness = value;
          return draft;
        });
      });
      brightnessInput.addEventListener('input', (event) => {
        const value = Math.min(255, Math.max(0, Math.round(Number(event.target.value))));
        event.target.value = String(value);
        pushBrightness(value);
      });
      brightnessWrap.append(brightnessInput, brightnessValue);

      const colorWrap = document.createElement('label');
      colorWrap.className = 'led-control';
      colorWrap.textContent = 'Color';
      const colorInput = document.createElement('input');
      colorInput.type = 'color';
      const colorValue = document.createElement('span');
      colorValue.className = 'led-value';
      const pushColor = runtime.createThrottle((value) => {
        const formatted =
          typeof value === 'string' && /^#([0-9a-fA-F]{6})$/.test(value)
            ? value.toUpperCase()
            : '#000000';
        colorValue.textContent = formatted;
        runtime.stage((draft) => {
          draft.led = draft.led || { brightness: 0, color: '#000000' };
          draft.led.color = formatted;
          return draft;
        });
      });
      colorInput.addEventListener('input', (event) => {
        pushColor(event.target.value);
      });
      colorWrap.append(colorInput, colorValue);

      container.append(brightnessWrap, colorWrap);
      state.mounted = true;
      state.brightnessInput = brightnessInput;
      state.brightnessValue = brightnessValue;
      state.colorInput = colorInput;
      state.colorValue = colorValue;
    }

    if (state.brightnessInput) {
      state.brightnessInput.value = String(initialBrightness);
    }
    if (state.brightnessValue) {
      state.brightnessValue.textContent = String(initialBrightness);
    }
    if (state.colorValue) {
      state.colorValue.textContent = initialColor.toUpperCase();
    }
    if (state.colorInput && document.activeElement !== state.colorInput) {
      state.colorInput.value = initialColor;
    }
  }

  return { render };
}
