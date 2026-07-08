import { IconLoader } from './iconLoader.js';

class InfiniteIconsPage {
  pageNum = -1;
  iconCount = 0;
  abortController = null;
  imgs = [];
  span = null;

  static SCROLL_ABORT = "Scrolling to a new page before last one was done loading";

  constructor(element) {
    this.element = element;
    this.onClick = this.clickOnImage.bind(this);
    this.element.addEventListener("click", this.onClick);
  }

  layout(iconCount) {
    if (this.iconCount !== iconCount) {
      this.iconCount = iconCount;
      this.pageNum = -1;

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

      this.imgs = Array.from(grid.children);
      this.span = span;
    }
  }

  clear() {
    this.imgs.forEach (img => img.src = "data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
  }

  async load(iconLoader, pageNum) {
    if (this.pageNum === pageNum) {
      return;
    }

    const controller = new AbortController();
    this.abortController?.abort(InfiniteIconsPage.SCROLL_ABORT);
    this.abortController = controller;

    this.pageNum = pageNum;

    const startIndex = pageNum * this.iconCount;

    try {
      // Load the icons in the grid
      let iconIndex = 0;
      for await (const icon of iconLoader.loadIcons(startIndex, this.iconCount, controller.signal)) {
        const img = this.imgs[iconIndex];
        if (img) {
          img.src = icon.dataUrl;
          img.title = icon.title;
          img.dataset.srcFile = icon.name;
        }
        iconIndex++;
      }

      // Update the icon number
      this.span.textContent = (startIndex + iconIndex).toLocaleString();
    } catch(e) {
      if (e !== InfiniteIconsPage.SCROLL_ABORT) {
        // SCROLL_ABORTs are frequent and expected, anything else not.
        throw e;
      }
    }
  }

  clickOnImage(e) {
    if (e.target.matches('.page img')) {
      const src = e.target.dataset.srcFile;
      console.log(`Copied image ${src} to clipboard`);
      navigator.clipboard.writeText(src);
    }
  }

  moveTo(top) {
    if (this.top !== top) {
      this.top = top;
      this.clear();
      this.element.style.top = top + "px";
      this.span.textContent = "";
    }
  }
}

class InfiniteIcons extends HTMLElement {
  static ICON_W = 64;
  static ICON_H = 64;

  static SPACING = 16;
  static EXTRA_ROWS = 10;
  static PAGE_GUTTER = 50;

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
    grid-template-columns: repeat(auto-fill,${InfiniteIcons.ICON_W}px);
    gap: ${InfiniteIcons.SPACING}px;
    padding: ${InfiniteIcons.SPACING/2}px 0;
  }

  infinite-icons.numbered .grid {
    margin-right: ${InfiniteIcons.PAGE_GUTTER}px;
  }

  infinite-icons img {
    width:${InfiniteIcons.ICON_W}px;
    height:${InfiniteIcons.ICON_W}px;
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

  static defaultIconLoader = new IconLoader("iconCollection/");

  iconLoader = InfiniteIcons.defaultIconLoader;

  evenPage = null;
  oddPage =  null;

  constructor() {
    super();
  }

  loadIcon(n) {
    const c  = document.createElement("canvas");
    c.width  = 32;
    c.height = 32;

    const g = c.getContext("2d");

    g.fillStyle="#fff";
    g.fillRect(0,0,c.width,c.height);

    g.fillStyle="#000";
    g.font="12px sans-serif";
    g.textAlign="center";
    g.textBaseline="middle";

    g.fillText(n, c.height/2, c.height/2);

    return c.toDataURL();
  }

  showVisiblePages() {
    const scrollY        = -this.container.getBoundingClientRect().top;
    const pageNum        = Math.max (0, Math.floor (scrollY / this.pageHeight));
    const pageIsEven     = pageNum % 2 === 0;
    const newEvenPageNum = pageIsEven ? pageNum     : pageNum + 1;
    const newOddPageNum  = pageIsEven ? pageNum + 1 : pageNum;

    this.evenPage.moveTo(newEvenPageNum * this.pageHeight);
    this.oddPage.moveTo(newOddPageNum * this.pageHeight);

    try {
      void this.evenPage.load(this.iconLoader, newEvenPageNum);
      void this.oddPage.load(this.iconLoader, newOddPageNum);
    } catch (e) {
    }
  }

  async updatePopulationCounts() {
    const totalIcons = await this.iconLoader.getTotalIcons();
    if (this.populateCount) {
      const countStr = totalIcons.toLocaleString();
      document.querySelectorAll(this.populateCount).forEach (el => {
          el.innerText = countStr;
        });
    }
  }

  installStyle() {
    const styleId = "infinite-icons-style";
    if (document.getElementById(styleId)) {
      return;
    }
    const style = document.createElement("style");
    style.id = styleId;
    style.textContent = InfiniteIcons.STYLE;
    document.head.appendChild(style);
  }

  async layout() {
    this.inited = true;

    const totalIcons = await this.iconLoader.getTotalIcons();
    const rowHeight = InfiniteIcons.ICON_H + InfiniteIcons.SPACING;

    const gridWidth = this.container.offsetWidth - (this.numbered ? InfiniteIcons.PAGE_GUTTER : 0);
    const cols = Math.max(1, Math.floor(gridWidth / (InfiniteIcons.ICON_W + InfiniteIcons.SPACING)));
    const rows = Math.ceil (totalIcons / cols);

    const rowsPerPage = Math.ceil(window.innerHeight / rowHeight) + InfiniteIcons.EXTRA_ROWS;

    const iconCount = cols * rowsPerPage;

    this.evenPage.layout(iconCount, this.numbered);
    this.oddPage.layout(iconCount, this.numbered);

    this.pageHeight = rowHeight * rowsPerPage;
    this.container.style.height = (rows * rowHeight) + "px";

    this.showVisiblePages(true);
  }

  connectedCallback() {
    this.installStyle();

    this.innerHTML = InfiniteIcons.TEMPLATE;

    if (this.numbered) {
      this.classList.add("numbered");
    } else {
      this.classList.remove("numbered");
    }

    this.container = this;
    this.evenPage = new InfiniteIconsPage(this.querySelector(".evenPage"));
    this.oddPage  = new InfiniteIconsPage(this.querySelector(".oddPage"));

    this.onScroll = () => this.showVisiblePages();
    this.onResize = () => this.layout();

    window.addEventListener("scroll", this.onScroll);
    window.addEventListener("resize", this.onResize);

    this.layout();
    this.updatePopulationCounts();
  }

  disconnectedCallback() {
    this.evenPage.element.removeEventListener("click", this.onClick);
    this.oddPage.element.removeEventListener("click", this.onClick);

    window.removeEventListener("scroll", this.onScroll);
    window.removeEventListener("resize", this.onResize);
  }

  // Attributes:

  static get observedAttributes() {
    return ["numbered","populateCount"];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (!this.inited) return;
    if (name === "numbered") {
      if (this.numbered) {
        this.classList.add("numbered");
      } else {
        this.classList.remove("numbered");
      }
      this.layout();
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
}

customElements.define("infinite-icons", InfiniteIcons);