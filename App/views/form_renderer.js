const DEFAULT_DEBOUNCE = 150;

function splitPath(path) {
  return path.split('.').map((segment) => {
    if (!segment) return segment;
    const index = Number(segment);
    return Number.isInteger(index) && String(index) === segment ? index : segment;
  });
}

function getValueAt(obj, path) {
  if (!obj) return undefined;
  const segments = splitPath(path);
  let cursor = obj;
  for (const segment of segments) {
    if (cursor === undefined || cursor === null) return undefined;
    cursor = cursor[segment];
  }
  return cursor;
}

function setValueAt(obj, path, value) {
  const segments = splitPath(path);
  let cursor = obj;
  for (let i = 0; i < segments.length; i += 1) {
    const segment = segments[i];
    if (i === segments.length - 1) {
      cursor[segment] = value;
      break;
    }
    if (cursor[segment] === undefined || cursor[segment] === null) {
      cursor[segment] = typeof segments[i + 1] === 'number' ? [] : {};
    }
    cursor = cursor[segment];
  }
}

function debounce(fn, delay) {
  let timer;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delay);
  };
}

function pathToId(path) {
  return `schema-${path.replace(/\./g, '-')}`;
}

function titleize(key) {
  return key
    .replace(/([A-Z])/g, ' $1')
    .replace(/[-_\.]/g, ' ')
    .replace(/^\s+|\s+$/g, '')
    .replace(/\w+/g, (word) => word[0].toUpperCase() + word.slice(1));
}

function createHelpBadge(message) {
  if (!message) return null;
  const badge = document.createElement('span');
  badge.className = 'help-badge';
  badge.textContent = 'i';
  badge.title = message;
  badge.setAttribute('aria-label', message);
  badge.setAttribute('role', 'img');
  return badge;
}

export class FormRenderer {
  constructor({ runtime, sections = [], debounceMs = DEFAULT_DEBOUNCE }) {
    this.runtime = runtime;
    this.sections = sections;
    this.debounceMs = debounceMs;
    this.schema = null;
    this.fields = new Map();
    // Per-path timers coalesce rapid UI edits into one outbound patch per field.
    this._patchSchedule = new Map();
  }

  updateSchema(schema) {
    this.schema = schema;
    this.renderSections();
  }

  renderSections() {
    if (!this.schema) return;
    this.sections.forEach((section) => {
      const target = section.target;
      target.innerHTML = '';
      const node = this.lookupNode(section.schemaPath);
      if (!node) return;
      const container = document.createElement('div');
      container.className = 'schema-section-render';
      const staged = this.runtime.getState().staged;
      const current = getValueAt(staged, section.schemaPath);
      this.buildNode(section.schemaPath, node, current, container);
      target.appendChild(container);
    });
  }

  updateValues() {
    const staged = this.runtime.getState().staged;
    for (const [path, control] of this.fields) {
      const value = getValueAt(staged, path);
      control.set(value);
    }
  }

  lookupNode(path) {
    const segments = path.split('.');
    let cursor = this.schema;
    for (const segment of segments) {
      if (!cursor || !cursor.properties) return null;
      cursor = cursor.properties[segment];
    }
    return cursor;
  }

  buildNode(basePath, schema, value, container) {
    if (!schema) return;
    const type = schema.type;
    if (type === 'object') {
      this.buildObject(basePath, schema, value, container);
    } else if (type === 'array') {
      this.buildArray(basePath, schema, value, container);
    } else {
      container.appendChild(this.createField(basePath, schema, value));
    }
  }

  buildObject(basePath, schema, value, container) {
    const entries = Object.entries(schema.properties ?? {}).filter(([, meta]) => meta);
    entries.forEach(([key, meta]) => {
      const path = basePath ? `${basePath}.${key}` : key;
      const fieldValue = value?.[key];
      if (meta.type === 'object' || meta.type === 'array') {
        const sub = document.createElement('section');
        sub.className = 'schema-section';
        const heading = document.createElement('h4');
        heading.textContent = meta.title ?? titleize(key);
        const headingHelp = createHelpBadge(meta.description);
        if (headingHelp) heading.appendChild(headingHelp);
        sub.appendChild(heading);
        if (meta.description) {
          const desc = document.createElement('p');
          desc.className = 'schema-description';
          desc.textContent = meta.description;
          sub.appendChild(desc);
        }
        const body = document.createElement('div');
        body.className = 'schema-section-body';
        this.buildNode(path, meta, fieldValue, body);
        sub.appendChild(body);
        container.appendChild(sub);
        return;
      }
      container.appendChild(this.createField(path, meta, fieldValue, meta.title ?? titleize(key)));
    });
  }

  buildArray(basePath, schema, value, container) {
    const items = Array.isArray(value) ? value : [];
    const count = Math.max(items.length, schema.minItems ?? 0);
    for (let index = 0; index < count; index += 1) {
      const entryValue = items[index];
      const detail = document.createElement('details');
      detail.className = 'schema-section';
      const summary = document.createElement('summary');
      const baseLabel = schema.title ?? titleize(basePath.split('.').pop() ?? basePath);
      summary.textContent = `${baseLabel} ${index + 1}`;
      detail.appendChild(summary);
      const body = document.createElement('div');
      body.className = 'schema-section-body';
      const childPath = `${basePath}.${index}`;
      this.buildNode(childPath, schema.items ?? {}, entryValue, body);
      detail.appendChild(body);
      container.appendChild(detail);
    }
  }

  createField(path, schema, value, labelText) {
    const wrapper = document.createElement('div');
    wrapper.className = 'schema-control';
    const label = document.createElement('label');
    const labelLine = document.createElement('span');
    labelLine.className = 'control-label';
    labelLine.textContent = labelText ?? schema.title ?? titleize(path.split('.').pop() ?? path);
    const labelHelp = createHelpBadge(schema.description);
    if (labelHelp) labelLine.appendChild(labelHelp);
    label.appendChild(labelLine);
    wrapper.appendChild(label);
    const action = document.createElement('div');
    action.className = 'schema-action';
    wrapper.appendChild(action);
    if (schema.description) {
      const desc = document.createElement('p');
      desc.className = 'schema-description';
      desc.textContent = schema.description;
      wrapper.appendChild(desc);
    }
    const type = schema.type;
    if (type === 'boolean') {
      const input = document.createElement('input');
      input.type = 'checkbox';
      input.checked = Boolean(value);
      action.appendChild(input);
      this.bindInput(
        path,
        schema,
        () => input.checked,
        (next) => {
          input.checked = Boolean(next);
        },
        input
      );
    } else if (schema.enum) {
      const select = document.createElement('select');
      schema.enum.forEach((opt) => {
        const option = document.createElement('option');
        option.value = opt;
        option.textContent = opt;
        if (value === opt) option.selected = true;
        select.appendChild(option);
      });
      action.appendChild(select);
      this.bindInput(path, schema, () => select.value, (next) => {
        select.value = next;
      }, select);
    } else if (type === 'number' || type === 'integer') {
      action.classList.add('schema-action-range');
      const slider = document.createElement('input');
      slider.type = 'range';
      slider.min = schema.minimum ?? 0;
      slider.max = schema.maximum ?? 100;
      const step = schema.multipleOf ?? (type === 'integer' ? 1 : 0.01);
      slider.step = step;
      slider.value = value ?? schema.default ?? slider.min;
      const numeric = document.createElement('input');
      numeric.type = 'number';
      if (schema.minimum !== undefined) numeric.min = schema.minimum;
      if (schema.maximum !== undefined) numeric.max = schema.maximum;
      numeric.step = step;
      numeric.value = slider.value;
      const valueLabel = document.createElement('span');
      valueLabel.className = 'schema-value';
      valueLabel.textContent = slider.value;
      action.append(slider, numeric, valueLabel);
    const applyVisualValue = (next) => {
        const numericValue = this.clampValue(schema, next);
        slider.value = String(numericValue);
        numeric.value = String(numericValue);
        valueLabel.textContent = String(numericValue);
        return numericValue;
      };
      const commit = (raw) => {
        const numericValue = applyVisualValue(raw);
        // Local stage first for instant UI feedback; device patch follows on the debounce lane.
        this.stageValue(path, numericValue);
        this.schedulePatch(path, numericValue);
      };
      slider.addEventListener('input', () => commit(slider.value));
      numeric.addEventListener('change', () => commit(numeric.value));
      const initial = applyVisualValue(value ?? schema.default ?? slider.min);
      this.fields.set(path, { set: (next) => applyVisualValue(next ?? initial) });
    } else {
      const input = document.createElement('input');
      input.type = 'text';
      input.value = value ?? '';
      action.appendChild(input);
      this.bindInput(path, schema, () => input.value, (next) => {
        input.value = next ?? '';
      }, input);
    }
    return wrapper;
  }

  bindInput(path, schema, getter, setter, element) {
      const control = element ?? document.createElement('div');
      // Debounce each field so rapid UI gestures stage safely before unit RPCs fire.
      const update = debounce((nextValue) => {
        const parsed = this.parseValue(schema, nextValue);
        this.stageValue(path, parsed);
        this.schedulePatch(path, parsed);
      }, this.debounceMs);
    const listener = () => update(getter());
    if (element) {
      element.addEventListener('change', listener);
      element.addEventListener('input', listener);
    } else {
      control.addEventListener('change', listener);
      control.addEventListener('input', listener);
    }
    this.fields.set(path, { set: setter || (() => {}) });
  }

  parseValue(schema, raw) {
    if (schema.type === 'number' || schema.type === 'integer') {
      return this.clampValue(schema, raw);
    }
    if (schema.type === 'boolean') {
      return Boolean(raw);
    }
    return raw;
  }

  clampValue(schema, raw) {
    let next = Number(raw);
    if (Number.isNaN(next)) {
      next = schema.default ?? 0;
    }
    if (schema.minimum !== undefined) {
      next = Math.max(next, schema.minimum);
    }
    if (schema.maximum !== undefined) {
      next = Math.min(next, schema.maximum);
    }
    if (schema.type === 'integer') {
      next = Math.round(next);
    }
    return next;
  }

  stageValue(path, value) {
    // Mutate staged config only; persistence happens when runtime apply/patch confirms.
    this.runtime.stage((draft) => {
      setValueAt(draft, path, value);
      return draft;
    });
  }

  schedulePatch(path, value) {
    const key = path;
    // Last-write-wins per path: replace any pending timer so only the freshest value is sent.
    if (this._patchSchedule.has(key)) clearTimeout(this._patchSchedule.get(key));
    const timer = setTimeout(() => {
      this._patchSchedule.delete(key);
      // Runtime already reports/rolls back transport errors; renderer keeps typing fluid.
      this.runtime.applyPatch(path, value).catch(() => {});
    }, this.debounceMs);
    this._patchSchedule.set(key, timer);
  }
}
