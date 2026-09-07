const ICON_DRAWINGS = Object.freeze({
  instrument: `
    <path d="M10 7v34M24 7v34M38 7v34" />
    <rect x="6" y="15" width="8" height="7" rx="2" />
    <rect x="20" y="28" width="8" height="7" rx="2" />
    <rect x="34" y="11" width="8" height="7" rx="2" />`,
  profile: `
    <path d="M13 10h25a4 4 0 0 1 4 4v24H17a4 4 0 0 1-4-4V10Z" />
    <path d="M13 17H8a3 3 0 0 0-3 3v21h28" />
    <path d="M21 25h13M21 31h9" />
    <circle cx="21" cy="17" r="2" />`,
  observe: `
    <rect x="5" y="9" width="38" height="29" rx="4" />
    <path d="M9 26h6l4-9 6 18 5-13 4 4h5" />
    <path d="M18 42h12" />`,
  evidence: `
    <path d="M11 6h20l7 7v25a4 4 0 0 1-4 4H11a4 4 0 0 1-4-4V10a4 4 0 0 1 4-4Z" />
    <path d="M30 6v9h8M14 20h13M14 26h9" />
    <circle cx="31" cy="32" r="6" />
    <path d="m35.5 36.5 6 6" />`,
  controls: `
    <circle cx="15" cy="16" r="7" />
    <circle cx="34" cy="31" r="7" />
    <path d="m15 16 4-4M34 31l-3 4M8 36h11M29 12h11" />`,
  assignments: `
    <circle cx="8" cy="13" r="3" /><circle cx="8" cy="35" r="3" />
    <circle cx="40" cy="13" r="3" /><circle cx="40" cy="35" r="3" />
    <path d="M11 13h8c8 0 3 22 13 22h5M11 35h7c7 0 4-22 14-22h5" />`,
  filter: `
    <path d="M5 11h38M5 39h38M8 16h9c7 0 9 3 12 10s5 9 11 9" />
    <circle cx="25" cy="21" r="3" />`,
  arg: `
    <path d="M5 14h7c5 0 7 3 10 10s5 10 10 10h11M5 34h7c5 0 7-3 10-10s5-10 10-10h11" />
    <circle cx="24" cy="24" r="4" />`,
  display: `
    <rect x="5" y="9" width="38" height="30" rx="4" />
    <path d="M10 28h6l4-10 7 15 5-10 6 5M17 43h14" />`,
  led: `
    <path d="M16 28c-3-2-5-6-5-10a13 13 0 0 1 26 0c0 4-2 8-5 10l-2 4H18l-2-4Z" />
    <path d="M19 37h10M21 42h6M24 1v5M5 18H1M47 18h-4M8 6l4 4M40 6l-4 4" />`,
  live: `
    <path d="M26 5 13 26h11l-2 17 13-23H24l2-15Z" />
    <path d="M7 39h8M35 9h7" />`,
  device: `
    <rect x="11" y="11" width="26" height="26" rx="4" />
    <path d="M17 24h4l3-7 5 14 3-7h5M16 5v6M24 5v6M32 5v6M16 37v6M24 37v6M32 37v6M5 16h6M5 24h6M5 32h6M37 16h6M37 24h6M37 32h6" />`,
  scope: `
    <path d="M5 26h7l4-12 7 23 7-18 5 7h8" />
    <path d="M6 10h36M6 42h36" />`,
  midi: `
    <circle cx="22" cy="24" r="15" />
    <circle cx="16" cy="19" r="1.7" />
    <circle cx="22" cy="16" r="1.7" />
    <circle cx="28" cy="19" r="1.7" />
    <circle cx="18" cy="27" r="1.7" />
    <circle cx="26" cy="27" r="1.7" />
    <path d="M37 24h7m-4-4 4 4-4 4" />`,
  matrix: `
    <circle cx="8" cy="12" r="3" /><circle cx="8" cy="36" r="3" />
    <circle cx="25" cy="24" r="4" /><circle cx="41" cy="12" r="3" /><circle cx="41" cy="36" r="3" />
    <path d="m11 13 10 8M11 35l10-8M29 22l9-8M29 26l9 8" />`,
  diff: `
    <path d="M8 8h14v32H8zM27 8h14v32H27z" />
    <path d="M12 16h6M15 13v6M31 16h6M12 27h6M31 27h6M31 33h6" />`,
  inspector: `
    <rect x="6" y="8" width="28" height="28" rx="4" />
    <path d="M12 16h14M12 23h9M12 30h6" />
    <circle cx="33" cy="33" r="7" /><path d="m38 38 5 5" />`,
  debug: `
    <rect x="5" y="8" width="38" height="32" rx="4" />
    <path d="m12 18 6 5-6 5M22 29h10" />`,
  arp: `
    <path d="M7 37h34M10 34V27h8v-7h8v-7h8V7" />
    <circle cx="14" cy="27" r="3" /><circle cx="22" cy="20" r="3" /><circle cx="30" cy="13" r="3" /><circle cx="38" cy="7" r="3" />`,
  routes: `
    <path d="M5 24c6-14 12-14 18 0s12 14 20 0" />
    <circle cx="7" cy="24" r="3" /><circle cx="41" cy="24" r="3" />
    <path d="M24 24v15h12m-4-4 4 4-4 4" />`,
  incoming: `
    <circle cx="34" cy="24" r="10" />
    <circle cx="31" cy="21" r="1.4" /><circle cx="37" cy="21" r="1.4" /><circle cx="34" cy="27" r="1.4" />
    <path d="M5 24h17m-5-6 6 6-6 6" />`,
  usb: `
    <path d="M24 42V13M24 13l-6 6M24 13l6 6M24 28l-8-6M24 34l8-6" />
    <path d="M21 8h6v5h-6z" /><circle cx="14" cy="21" r="3" /><path d="M29 25h6v6h-6z" />`,
  clock: `
    <circle cx="24" cy="24" r="18" />
    <path d="M24 13v12l8 5M6 24H2M46 24h-4" />`
});

export function explainerIconMarkup(name, className = '') {
  const drawing = ICON_DRAWINGS[name] ?? ICON_DRAWINGS.instrument;
  return `
    <svg class="explainer-icon ${className}" viewBox="0 0 48 48" aria-hidden="true" focusable="false" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      ${drawing}
    </svg>
  `;
}

export function createExplainerIcon(document, name, className = '') {
  const template = document.createElement('template');
  template.innerHTML = explainerIconMarkup(name, className).trim();
  return template.content.firstElementChild;
}
