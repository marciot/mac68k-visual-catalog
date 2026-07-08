class InfiniteIconsDefaultLoader {
  #makeIcon(n) {
    const c  = document.createElement("canvas");
    c.width  = InfiniteIcons.ICON_NATIVE_W;
    c.height = InfiniteIcons.ICON_NATIVE_H;

    const g = c.getContext("2d");

    g.fillStyle="#fff";
    g.fillRect(0,0,c.width,c.height);

    g.fillStyle="#000";
    g.font="12px sans-serif";
    g.textAlign="center";
    g.textBaseline="middle";

    g.fillText(n, c.height/2, c.height/2);

    const dataUrl = c.toDataURL();
    const name = n.toString() + ".png";
    const title = n.toString();
    return {name, dataUrl, title};
  }

  getTotalIcons() {
    return 10000;
  }

  *loadIcons(startIndex, numberOfIcons, signal) {
    while ((numberOfIcons--) && (startIndex < this.getTotalIcons())) {
      yield this.#makeIcon(startIndex++);
    }
  }
}

class InfiniteIconsPage {
  pageNum = null;
  pageTop = 0;
  iconCount = 0;
  abortController = null;
  images = [];
  span = null;

  static EMPTY_IMAGE = "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

  constructor(element) {
    this.element = element;
  }

  updateLayout(iconCount) {
    if (this.iconCount !== iconCount) {
      this.iconCount = iconCount;
      this.pageNum = null;

      const grid = document.createElement("div");
      for (let i = 0; i < iconCount; i++) {
        grid.appendChild(document.createElement("img"));
      }
      const span = document.createElement("span");

      grid.className = "grid";
      span.className = "iconNum";

      const fragment = document.createDocumentFragment();
      fragment.appendChild(grid);
      fragment.appendChild(span);
      this.element.replaceChildren(fragment);

      this.images = Array.from(grid.children);
      this.span = span;
    }
  }

  clear() {
    this.images.forEach (img => img.src = InfiniteIconsPage.EMPTY_IMAGE);
  }

  async load(iconLoader, pageNum) {
    if (this.pageNum === pageNum) {
      return;
    }

    const controller = new AbortController();
    this.abortController?.abort();
    this.abortController = controller;

    this.pageNum = pageNum;

    const startIndex = pageNum * this.iconCount;

    try {
      // Load the icons in the grid
      let iconIndex = 0;
      for await (const icon of iconLoader.loadIcons(startIndex, this.iconCount, controller.signal)) {
        const img = this.images[iconIndex];
        img.src = icon.dataUrl;
        img.title = icon.title;
        img.dataset.srcFile = icon.name;
        iconIndex++;
      }

      // Update the icon number
      this.span.textContent = (startIndex + iconIndex).toLocaleString();
    } catch(e) {
      if (e.name !== "AbortError") {
        // AbortErrors are expected during scrolling
        throw e;
      }
    } finally {
      if (this.abortController === controller) {
        this.abortController = null;
      }
    }
  }

  showAt(pageTop) {
    if (this.pageTop !== pageTop) {
      this.pageTop = pageTop;
      this.clear();
      this.element.style.top = pageTop + "px";
      this.span.textContent = "";
    }
  }
}

class InfiniteIcons extends HTMLElement {
  static ICON_NATIVE_W = 32;
  static ICON_NATIVE_H = 32;
  static ICON_SCALED_W = 64;
  static ICON_SCALED_H = 64;

  static SPACING       = 16;
  static EXTRA_ROWS    = 0;
  static PAGE_GUTTER   = 50;

  static TEMPLATE = `
  <div class="page evenPage"></div>
  <div class="page oddPage"></div>
`;

  static STYLE = `
  infinite-icons {
    display: block;
    position: relative;
    overflow: hidden;
    box-sizing: border-box;
  }

  infinite-icons .page {
    position: absolute;
    left: 0;
    right: 0;
    background: transparent;
  }

  infinite-icons .grid {
    display: grid;
    justify-content: space-between;
    grid-template-columns: repeat(auto-fill,${InfiniteIcons.ICON_SCALED_W}px);
    gap: ${InfiniteIcons.SPACING}px;
    padding: ${InfiniteIcons.SPACING/2}px 0;
  }

  infinite-icons.numbered .grid {
    margin-right: ${InfiniteIcons.PAGE_GUTTER}px;
  }

  infinite-icons img {
    width:${InfiniteIcons.ICON_SCALED_W}px;
    height:${InfiniteIcons.ICON_SCALED_W}px;
    flex:0 0 auto;
    image-rendering:crisp-edges;
  }

  infinite-icons img:hover {
    background:#E6E6FA;
    outline:2px solid #E6E6FA;
  }

  infinite-icons .iconNum {
    position: absolute;
    right: 0;
    bottom: calc(-5px -10pt);

    color:gray;
    font-size:10pt;
  }

  infinite-icons:not(.numbered) .iconNum {
    display: none;
  }

  infinite-icons.numbered .page::after {
    content: "";
    position:absolute;
    left:0;
    right:0;
    bottom: 2px;
    border-bottom:1px dashed black;
  }
`;

  static defaultIconLoader = null;

  iconLoader = null;

  evenPage = null;
  oddPage =  null;
  pageHeight = -1;

  constructor() {
    super();
  }

  static computeLayout(viewportHeight, containerWidth, iconCount, isNumbered) {
    const rowHeight = InfiniteIcons.ICON_SCALED_H + InfiniteIcons.SPACING;

    const gridWidth = containerWidth - (isNumbered ? InfiniteIcons.PAGE_GUTTER : 0);
    const cols = Math.max(1, Math.floor(gridWidth / (InfiniteIcons.ICON_SCALED_W + InfiniteIcons.SPACING)));
    const rows = Math.ceil (iconCount / cols);

    const rowsPerPage = Math.ceil(viewportHeight / rowHeight) + InfiniteIcons.EXTRA_ROWS;

    const pageHeight = rowHeight * rowsPerPage;
    const iconsPerPage = cols * rowsPerPage;
    const totalHeight = rows * rowHeight;

    return {pageHeight, iconsPerPage, totalHeight};
  }

  static computeVisiblePages(pageHeight, scrollY) {
    const pageNum  = Math.max (0, Math.floor (scrollY / pageHeight));
    const isEven   = pageNum % 2 === 0;
    const even     = isEven ? pageNum     : pageNum + 1;
    const odd      = isEven ? pageNum + 1 : pageNum;
    return {even, odd};
  }

  showVisiblePages() {
    if (!this.pageHeight) return;
    const pageHeight = this.pageHeight;
    const scrollY  = -this.getBoundingClientRect().top;
    const visible = InfiniteIcons.computeVisiblePages(pageHeight, scrollY);

    this.evenPage.showAt(visible.even * pageHeight);
    this.oddPage.showAt( visible.odd  * pageHeight);

    void this.evenPage.load(this.iconLoader, visible.even);
    void this.oddPage.load(this.iconLoader, visible.odd);
  }

  async updateLayout() {
    if(!this.iconLoader) return;
    const iconCount = await this.iconLoader.getTotalIcons();

    const layout = InfiniteIcons.computeLayout(window.innerHeight, this.clientWidth, iconCount, this.numbered);

    this.evenPage.updateLayout(layout.iconsPerPage, this.numbered);
    this.oddPage.updateLayout(layout.iconsPerPage, this.numbered);

    this.style.height = layout.totalHeight + "px";
    this.pageHeight = layout.pageHeight;

    this.showVisiblePages();
  }

  initializeDOM() {
    const styleId = "infinite-icons-style";
    if (!document.getElementById(styleId)) {
      const style = document.createElement("style");
      style.id = styleId;
      style.textContent = InfiniteIcons.STYLE;
      document.head.appendChild(style);
    }

    this.innerHTML = InfiniteIcons.TEMPLATE;
    if (this.numbered) {
      this.classList.add("numbered");
    } else {
      this.classList.remove("numbered");
    }
    this.evenPage = new InfiniteIconsPage(this.querySelector(".evenPage"));
    this.oddPage  = new InfiniteIconsPage(this.querySelector(".oddPage"));
  }

  initializeEventHandlers() {
    this.onScroll = this.showVisiblePages.bind(this);
    this.onResize = this.updateLayout.bind(this);
    this.onClick  = this.clickOnImage.bind(this);

    window.addEventListener("scroll", this.onScroll);
    window.addEventListener("resize", this.onResize);
    this.addEventListener("click", this.onClick);

    if (typeof ResizeObserver !== 'undefined') {
      this.resizeObserver = new ResizeObserver(this.onResize);
      this.resizeObserver.observe(this);
    }
  }

  async connectedCallback() {
    this.iconLoader = new InfiniteIcons.defaultIconLoader(this.src);

    this.initializeDOM();
    this.initializeEventHandlers();

    await this.updateLayout();
    void this.updatePopulationCounts();
  }

  disconnectedCallback() {
    window.removeEventListener("scroll", this.onScroll);
    window.removeEventListener("resize", this.onResize);
    this.removeEventListener("click", this.onClick);
    this.resizeObserver?.disconnect();
    this.evenPage.abortController?.abort();
    this.oddPage.abortController?.abort();
  }

  async updatePopulationCounts() {
    if (this.populateCount) {
      const iconCount = await this.iconLoader.getTotalIcons();
      const countStr = iconCount.toLocaleString();
      document.querySelectorAll(this.populateCount).forEach (el => {
          el.innerText = countStr;
        });
    }
  }

  clickOnImage(e) {
    if (e.target.matches('.page img')) {
      const src = e.target.dataset.srcFile;
      console.log(`Copied image ${src} to clipboard`);
      navigator.clipboard.writeText(src);
    }
  }

  // Attributes:

  static get observedAttributes() {
    return ["numbered","populateCount", "src"];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name === "numbered") {
      if (this.numbered) {
        this.classList.add("numbered");
      } else {
        this.classList.remove("numbered");
      }
      this.updateLayout();
    }
    if (name === "populateCount") {
      this.updatePopulationCounts()
    }
  }

  get numbered() {
    return this.hasAttribute("numbered");
  }

  get populateCount() {
    return this.getAttribute("populateCount");
  }

  get src() {
    return this.getAttribute("src");
  }
}

try {
  const iconLoader = await import('./iconLoader.js');
  InfiniteIcons.defaultIconLoader = iconLoader.IconLoader;
} catch (err) {
  console.warn('Unable to find IconLoader module, will use fallback: ', err.message);
  InfiniteIcons.defaultIconLoader = InfiniteIconsDefaultLoader;
}

customElements.define("infinite-icons", InfiniteIcons);