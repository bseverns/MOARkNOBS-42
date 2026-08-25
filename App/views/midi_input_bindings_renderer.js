const PORT_OPTIONS = [
  { value: 'any', label: 'DIN + USB' },
  { value: 'din', label: 'DIN only' },
  { value: 'usb', label: 'USB only' }
];

const MESSAGE_OPTIONS = [{ value: 'cc7', label: 'Control Change (7-bit)' }];

const MODE_OPTIONS = [
  { value: 'absolute', label: 'Continuous' },
  { value: 'momentary', label: 'Momentary button' },
  { value: 'toggle', label: 'Toggle button' }
];

const PICKUP_OPTIONS = [
  { value: 'soft', label: 'Soft pickup' },
  { value: 'jump', label: 'Jump immediately' }
];

const PERFORMANCE_DESTINATIONS = [
  { value: 'arp.swing', label: 'Arpeggiator · Swing' },
  { value: 'arp.gate', label: 'Arpeggiator · Gate' },
  { value: 'note.velocity_shift', label: 'Notes · Velocity shift' },
  { value: 'note.probability', label: 'Notes · Probability' },
  { value: 'jitter.depth', label: 'Jitter · Depth' },
  { value: 'jitter.smoothness', label: 'Jitter · Smoothness' }
];

function domId(path) {
  return `midi-binding-${path.replace(/[^a-zA-Z0-9_-]/g, '-')}`;
}

function selectedLabel(options, value) {
  for (const option of options) {
    if (option.options) {
      const nested = selectedLabel(option.options, value);
      if (nested) return nested;
    } else if (option.value === value) {
      return option.label;
    }
  }
  return '';
}

function destinationOptions(slotCount, currentValue) {
  const options = [
    {
      label: 'Slots',
      options: Array.from({ length: slotCount }, (_, index) => ({
        value: `slot.${index}.value`,
        label: `Slot ${index + 1} · Value`
      }))
    },
    { label: 'Performance parameters', options: PERFORMANCE_DESTINATIONS }
  ];
  if (!selectedLabel(options, currentValue) && currentValue) {
    options.push({
      label: 'Current device value',
      options: [{ value: currentValue, label: currentValue }]
    });
  }
  return options;
}

function appendOptions(select, options, value) {
  for (const option of options) {
    if (option.options) {
      const group = document.createElement('optgroup');
      group.label = option.label;
      appendOptions(group, option.options, value);
      select.appendChild(group);
      continue;
    }
    const element = document.createElement('option');
    element.value = option.value;
    element.textContent = option.label;
    element.selected = option.value === value;
    select.appendChild(element);
  }
}

function createField(
  renderer,
  { path, schema, value, label, options, help, className = '', onInput }
) {
  const wrapper = document.createElement('div');
  wrapper.className = `midi-binding-field ${className}`.trim();
  wrapper.dataset.deviceConfigPath = path;

  const labelElement = document.createElement('label');
  labelElement.className = 'control-label';
  labelElement.htmlFor = domId(path);
  labelElement.textContent = label;
  wrapper.appendChild(labelElement);

  let input;
  if (options) {
    input = document.createElement('select');
    appendOptions(input, options, value ?? schema.default);
  } else {
    input = document.createElement('input');
    input.type = schema.type === 'integer' || schema.type === 'number' ? 'number' : 'text';
    input.value = value ?? schema.default ?? '';
    if (schema.minimum !== undefined) input.min = schema.minimum;
    if (schema.maximum !== undefined) input.max = schema.maximum;
    input.step = schema.multipleOf ?? (schema.type === 'integer' ? 1 : 'any');
    if (input.type === 'number') input.inputMode = 'numeric';
  }
  input.id = domId(path);
  input.name = path;
  input.dataset.configPath = path;
  wrapper.appendChild(input);

  if (help) {
    const helpElement = document.createElement('small');
    helpElement.className = 'midi-binding-field-help';
    helpElement.id = `${domId(path)}-help`;
    helpElement.textContent = help;
    wrapper.appendChild(helpElement);
    input.setAttribute('aria-describedby', helpElement.id);
  }

  renderer.bindInput(
    path,
    schema,
    () => input.value,
    (next) => {
      input.value = next ?? schema.default ?? '';
      onInput?.();
    },
    input
  );
  if (onInput) {
    input.addEventListener('input', onInput);
    input.addEventListener('change', onInput);
  }
  return { wrapper, input };
}

function createGroup(title, description, className, id) {
  const group = document.createElement('section');
  group.className = `midi-binding-group ${className}`;
  group.setAttribute('role', 'group');
  const heading = document.createElement('h4');
  heading.id = domId(id);
  heading.textContent = title;
  group.setAttribute('aria-labelledby', heading.id);
  group.appendChild(heading);
  const copy = document.createElement('p');
  copy.className = 'midi-binding-group-copy';
  copy.textContent = description;
  group.appendChild(copy);
  return group;
}

function appendFallbackProperties(renderer, group, path, schema, value, handledKeys) {
  for (const [key, propertySchema] of Object.entries(schema?.properties ?? {})) {
    if (handledKeys.has(key)) continue;
    const container = document.createElement('div');
    container.className = 'midi-binding-fallback';
    renderer.buildNode(`${path}.${key}`, propertySchema, value?.[key], container);
    group.appendChild(container);
  }
}

function displayDestination(value) {
  const slot = /^slot\.(\d+)\.value$/.exec(value ?? '');
  if (slot) return `Slot ${Number(slot[1]) + 1} value`;
  return selectedLabel([{ options: PERFORMANCE_DESTINATIONS }], value) || value || 'Choose destination';
}

function reindexOpenItems(openItems, removedIndex) {
  const next = [...openItems]
    .filter((index) => index !== removedIndex)
    .map((index) => (index > removedIndex ? index - 1 : index));
  openItems.clear();
  next.forEach((index) => openItems.add(index));
}

export function buildMidiInputBindings(renderer, basePath, schema, value, container) {
  const items = Array.isArray(value) ? value : [];
  const maxItems = schema.maxItems ?? Number.POSITIVE_INFINITY;
  const slotCount = renderer.schema?.properties?.slots?.maxItems ?? 42;

  const toolbar = document.createElement('div');
  toolbar.className = 'midi-binding-toolbar';
  const count = document.createElement('p');
  count.className = 'midi-binding-count';
  count.setAttribute('aria-live', 'polite');
  count.innerHTML = `<strong>${items.length}</strong> of ${maxItems} bindings`;
  toolbar.appendChild(count);

  if (items.length < maxItems) {
    const add = document.createElement('button');
    add.type = 'button';
    add.className = 'schema-array-add midi-binding-add';
    add.textContent = 'Add binding';
    add.addEventListener('click', () => {
      const stagedItems = renderer.getValueAt(renderer.runtime.getState().staged, basePath);
      const next = Array.isArray(stagedItems) ? stagedItems.slice() : [];
      next.push(renderer.defaultValueForSchema(schema.items ?? {}));
      renderer.midiBindingOpenItems.clear();
      renderer.midiBindingOpenItems.add(next.length - 1);
      renderer.stageValue(basePath, next);
      renderer.renderSections();
      renderer.schedulePatch(basePath, next);
    });
    toolbar.appendChild(add);
  } else {
    const limit = document.createElement('span');
    limit.className = 'midi-binding-limit';
    limit.textContent = 'Route limit reached';
    toolbar.appendChild(limit);
  }
  container.appendChild(toolbar);

  if (!items.length) {
    const empty = document.createElement('div');
    empty.className = 'midi-binding-empty';
    empty.innerHTML =
      '<strong>No bindings in this profile</strong><span>Add a route to turn an incoming MIDI CC into a slot value or performance control.</span>';
    container.appendChild(empty);
    return;
  }

  const list = document.createElement('div');
  list.className = 'midi-binding-list';
  items.forEach((binding, index) => {
    const itemPath = `${basePath}.${index}`;
    const itemSchema = schema.items ?? {};
    const itemProperties = itemSchema.properties ?? {};
    const sourceSchema = itemProperties.source ?? { type: 'object', properties: {} };
    const sourceProperties = sourceSchema.properties ?? {};
    const source = binding?.source ?? {};
    const outputRange = Array.isArray(binding?.outputRange) ? binding.outputRange : [0, 127];

    const detail = document.createElement('details');
    detail.className = 'midi-binding-card';
    detail.open = renderer.midiBindingOpenItems.has(index);
    detail.addEventListener('toggle', () => {
      if (detail.open) renderer.midiBindingOpenItems.add(index);
      else renderer.midiBindingOpenItems.delete(index);
    });

    const summary = document.createElement('summary');
    summary.className = 'midi-binding-summary';
    const routeNumber = document.createElement('span');
    routeNumber.className = 'midi-binding-route-number';
    routeNumber.textContent = `Route ${index + 1}`;
    const routeText = document.createElement('strong');
    routeText.className = 'midi-binding-route-text';
    const routeMeta = document.createElement('span');
    routeMeta.className = 'midi-binding-route-meta';
    summary.append(routeNumber, routeText, routeMeta);
    detail.appendChild(summary);

    const body = document.createElement('div');
    body.className = 'midi-binding-body';
    const groups = document.createElement('div');
    groups.className = 'midi-binding-groups';

    const sourceGroup = createGroup(
      'Incoming CC',
      'Choose the port, channel, and CC number this route listens for.',
      'midi-binding-source',
      `${itemPath}.source-group`
    );
    const sourceFields = document.createElement('div');
    sourceFields.className = 'midi-binding-field-grid midi-binding-source-grid';

    let portInput;
    let channelInput;
    let numberInput;
    let destinationInput;
    let modeInput;
    let pickupInput;
    let minInput;
    let maxInput;

    const refreshSummary = () => {
      if (!routeText) return;
      const port = selectedLabel(PORT_OPTIONS, portInput?.value ?? source.port) || 'Any port';
      const channel = channelInput?.value ?? source.channel ?? 1;
      const number = numberInput?.value ?? source.number ?? 0;
      const destination = displayDestination(destinationInput?.value ?? binding?.destination);
      const mode = selectedLabel(MODE_OPTIONS, modeInput?.value ?? binding?.mode) || 'Continuous';
      const minimum = minInput?.value ?? outputRange[0] ?? 0;
      const maximum = maxInput?.value ?? outputRange[1] ?? 127;
      routeText.textContent = `${port} · Ch ${channel} · CC ${number} → ${destination}`;
      routeMeta.textContent = `${mode} · ${minimum}–${maximum}`;
      if (pickupInput) {
        const pickupActive = (modeInput?.value ?? binding?.mode) === 'absolute';
        pickupInput.disabled = !pickupActive;
        pickupInput.closest('.midi-binding-field')?.toggleAttribute(
          'data-inactive',
          !pickupActive
        );
      }
    };

    ({ input: portInput } = createField(renderer, {
      path: `${itemPath}.source.port`,
      schema: sourceProperties.port ?? { type: 'string', enum: ['any', 'din', 'usb'] },
      value: source.port,
      label: 'Input port',
      options: PORT_OPTIONS,
      onInput: refreshSummary
    }));
    sourceFields.appendChild(portInput.closest('.midi-binding-field'));

    ({ input: channelInput } = createField(renderer, {
      path: `${itemPath}.source.channel`,
      schema: sourceProperties.channel ?? { type: 'integer', minimum: 1, maximum: 16 },
      value: source.channel,
      label: 'MIDI channel',
      onInput: refreshSummary
    }));
    sourceFields.appendChild(channelInput.closest('.midi-binding-field'));

    ({ input: numberInput } = createField(renderer, {
      path: `${itemPath}.source.number`,
      schema: sourceProperties.number ?? { type: 'integer', minimum: 0, maximum: 127 },
      value: source.number,
      label: 'CC number',
      onInput: refreshSummary
    }));
    sourceFields.appendChild(numberInput.closest('.midi-binding-field'));
    const messageSchema = sourceProperties.type ?? { type: 'string', enum: ['cc7'] };
    if ((messageSchema.enum?.length ?? 0) > 1) {
      const messageOptions = messageSchema.enum.map((value) =>
        MESSAGE_OPTIONS.find((option) => option.value === value) ?? { value, label: value }
      );
      const messageField = createField(renderer, {
        path: `${itemPath}.source.type`,
        schema: messageSchema,
        value: source.type,
        label: 'Message',
        options: messageOptions
      });
      sourceFields.appendChild(messageField.wrapper);
    } else {
      sourceGroup.dataset.deviceConfigPath = `${itemPath}.source.type`;
    }
    sourceGroup.appendChild(sourceFields);
    appendFallbackProperties(
      renderer,
      sourceGroup,
      `${itemPath}.source`,
      sourceSchema,
      source,
      new Set(['port', 'type', 'channel', 'number'])
    );
    groups.appendChild(sourceGroup);

    const destinationGroup = createGroup(
      'Destination',
      'Send the normalized value to one slot or an internal performance parameter.',
      'midi-binding-destination',
      `${itemPath}.destination-group`
    );
    ({ input: destinationInput } = createField(renderer, {
      path: `${itemPath}.destination`,
      schema: itemProperties.destination ?? { type: 'string' },
      value: binding?.destination,
      label: 'Control target',
      options: destinationOptions(slotCount, binding?.destination),
      onInput: refreshSummary
    }));
    destinationGroup.appendChild(destinationInput.closest('.midi-binding-field'));
    groups.appendChild(destinationGroup);

    const responseGroup = createGroup(
      'Response',
      'Set continuous or button behavior, then constrain the outgoing MIDI-value range.',
      'midi-binding-response',
      `${itemPath}.response-group`
    );
    const responseFields = document.createElement('div');
    responseFields.className = 'midi-binding-field-grid midi-binding-response-grid';
    ({ input: modeInput } = createField(renderer, {
      path: `${itemPath}.mode`,
      schema: itemProperties.mode ?? { type: 'string' },
      value: binding?.mode,
      label: 'Interaction',
      options: MODE_OPTIONS,
      onInput: refreshSummary
    }));
    responseFields.appendChild(modeInput.closest('.midi-binding-field'));

    ({ input: minInput } = createField(renderer, {
      path: `${itemPath}.outputRange.0`,
      schema: itemProperties.outputRange?.items ?? {
        type: 'integer',
        minimum: 0,
        maximum: 127
      },
      value: outputRange[0],
      label: 'Output minimum',
      onInput: refreshSummary
    }));
    responseFields.appendChild(minInput.closest('.midi-binding-field'));

    ({ input: maxInput } = createField(renderer, {
      path: `${itemPath}.outputRange.1`,
      schema: itemProperties.outputRange?.items ?? {
        type: 'integer',
        minimum: 0,
        maximum: 127
      },
      value: outputRange[1],
      label: 'Output maximum',
      onInput: refreshSummary
    }));
    responseFields.appendChild(maxInput.closest('.midi-binding-field'));

    ({ input: pickupInput } = createField(renderer, {
      path: `${itemPath}.pickup`,
      schema: itemProperties.pickup ?? { type: 'string' },
      value: binding?.pickup,
      label: 'Takeover',
      options: PICKUP_OPTIONS,
      help: 'Used by Continuous mode. Soft pickup prevents value jumps.',
      onInput: refreshSummary
    }));
    responseFields.appendChild(pickupInput.closest('.midi-binding-field'));
    responseGroup.appendChild(responseFields);
    groups.appendChild(responseGroup);

    appendFallbackProperties(
      renderer,
      groups,
      itemPath,
      itemSchema,
      binding,
      new Set(['source', 'destination', 'mode', 'outputRange', 'pickup'])
    );
    body.appendChild(groups);

    const remove = document.createElement('button');
    remove.type = 'button';
    remove.className = 'schema-array-remove midi-binding-remove';
    remove.textContent = 'Remove route';
    remove.setAttribute('aria-label', `Remove MIDI input route ${index + 1}`);
    remove.addEventListener('click', () => {
      const stagedItems = renderer.getValueAt(renderer.runtime.getState().staged, basePath);
      const next = Array.isArray(stagedItems) ? stagedItems.slice() : [];
      next.splice(index, 1);
      reindexOpenItems(renderer.midiBindingOpenItems, index);
      renderer.stageValue(basePath, next);
      renderer.renderSections();
      renderer.schedulePatch(basePath, next);
    });
    body.appendChild(remove);

    const keepRangeOrdered = (changed, peer, peerPath, direction) => {
      const changedValue = renderer.clampValue(itemProperties.outputRange?.items ?? {}, changed.value);
      const peerValue = renderer.clampValue(itemProperties.outputRange?.items ?? {}, peer.value);
      const invalid = direction === 'minimum' ? changedValue > peerValue : changedValue < peerValue;
      if (!invalid) return;
      peer.value = String(changedValue);
      renderer.stageValue(peerPath, changedValue);
      renderer.schedulePatch(peerPath, changedValue);
      refreshSummary();
    };
    minInput.addEventListener('input', () =>
      keepRangeOrdered(minInput, maxInput, `${itemPath}.outputRange.1`, 'minimum')
    );
    maxInput.addEventListener('input', () =>
      keepRangeOrdered(maxInput, minInput, `${itemPath}.outputRange.0`, 'maximum')
    );

    detail.appendChild(body);
    list.appendChild(detail);
    refreshSummary();
  });
  container.appendChild(list);
}
