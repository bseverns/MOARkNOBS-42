const sharedResizeObserver =
  typeof ResizeObserver === 'function'
    ? new ResizeObserver((entries) => {
        for (const entry of entries) {
          entry.target.__resizeCallback?.(entry);
        }
      })
    : null;

export class VirtualGrid {
  constructor(container, { columns, rowHeight, render }) {
    this.container = container;
    this.columns = columns;
    this.rowHeight = rowHeight;
    this.renderItem = render;
    this.data = [];
    this.pool = [];
    this.viewport = document.createElement('div');
    this.viewport.className = 'virtual-grid';
    if (this.container) {
      this.container.style.position = 'relative';
      this.container.style.overflowY = 'auto';
      this.container.appendChild(this.viewport);
      this.container.__resizeCallback = () => this.compute();
      sharedResizeObserver?.observe(this.container);
    }
    this.viewport.style.position = 'relative';
    this.viewport.style.width = '100%';
    this.container?.addEventListener('scroll', () => this.render());
  }

  setData(data) {
    this.data = data || [];
    this.compute();
  }

  compute() {
    if (!this.container) return;
    const visibleRows = Math.ceil(this.container.clientHeight / this.rowHeight) + 2;
    const needed = visibleRows * this.columns;
    while (this.pool.length < needed) {
      const el = document.createElement('div');
      el.className = 'virtual-item';
      el.style.position = 'absolute';
      this.viewport.appendChild(el);
      this.pool.push(el);
    }
    this.render();
  }

  render() {
    if (!this.container) return;
    const scrollTop = this.container.scrollTop;
    const firstRow = Math.max(0, Math.floor(scrollTop / this.rowHeight) - 1);
    const startIndex = firstRow * this.columns;
    this.viewport.style.height = `${Math.ceil(this.data.length / this.columns) * this.rowHeight}px`;
    this.pool.forEach((el, idx) => {
      const dataIndex = startIndex + idx;
      if (dataIndex >= this.data.length) {
        el.style.display = 'none';
        return;
      }
      el.style.display = '';
      const row = Math.floor(dataIndex / this.columns);
      const column = dataIndex % this.columns;
      el.style.width = `${100 / this.columns}%`;
      el.style.transform = `translate(${column * 100}%, ${row * this.rowHeight}px)`;
      el.style.height = `${this.rowHeight}px`;
      el.dataset.index = String(dataIndex);
      this.renderItem(el, dataIndex, this.data[dataIndex]);
    });
  }

  scrollToIndex(index) {
    if (!this.container) return;
    const row = Math.floor(index / this.columns);
    const target = row * this.rowHeight;
    this.container.scrollTo({ top: target, behavior: 'smooth' });
  }

  highlight(index) {
    this.pool.forEach((el) => {
      el.classList.toggle('selected', Number(el.dataset.index) === index);
    });
  }

  updateTelemetry(values) {
    this.pool.forEach((el) => {
      if (el.dataset.index === undefined) return;
      const idx = Number(el.dataset.index);
      const value = values[idx] ?? 0;
      el.dataset.value = value;
    });
  }
}

export class VirtualList {
  constructor(container, { itemHeight, render }) {
    this.container = container;
    this.itemHeight = itemHeight;
    this.renderItem = render;
    this.data = [];
    this.pool = [];
    this.viewport = document.createElement('div');
    this.viewport.className = 'virtual-list';
    if (this.container) {
      this.container.style.position = 'relative';
      this.container.style.overflowY = 'auto';
      this.container.appendChild(this.viewport);
      this.container.__resizeCallback = () => this.compute();
      sharedResizeObserver?.observe(this.container);
    }
    this.viewport.style.position = 'relative';
    this.viewport.style.width = '100%';
    this.container?.addEventListener('scroll', () => this.render());
  }

  setData(data) {
    this.data = data || [];
    this.compute();
  }

  compute() {
    if (!this.container) return;
    const visible = Math.ceil(this.container.clientHeight / this.itemHeight) + 2;
    while (this.pool.length < visible) {
      const el = document.createElement('div');
      el.className = 'virtual-row';
      el.style.position = 'absolute';
      this.viewport.appendChild(el);
      this.pool.push(el);
    }
    this.render();
  }

  render() {
    if (!this.container) return;
    const scrollTop = this.container.scrollTop;
    const first = Math.max(0, Math.floor(scrollTop / this.itemHeight) - 1);
    this.viewport.style.height = `${this.data.length * this.itemHeight}px`;
    this.pool.forEach((el, idx) => {
      const dataIndex = first + idx;
      if (dataIndex >= this.data.length) {
        el.style.display = 'none';
        return;
      }
      el.style.display = '';
      el.style.width = '100%';
      el.style.transform = `translateY(${dataIndex * this.itemHeight}px)`;
      this.renderItem(el, dataIndex, this.data[dataIndex]);
    });
  }
}
