import { IconLoader } from './iconLoader.js';

const infiniteIconsTemplate = `
  <div class="page evenPage"></div>
  <div class="page oddPage"></div>
`;

const infiniteIconsStyle = `
   infinite-icons {
    position: relative;
  }

  infinite-icons .page {
    position: absolute;
    left: 0;
    right: 0;

    display:flex;
    flex-direction:column;
    gap:16px;
    padding-block:8px;
    border-bottom:1px dashed black;
  }

  infinite-icons .row {
    display:flex;
    justify-content:space-between;
    align-items:center;
  }

  infinite-icons .row img {
    width:64px;
    height:64px;
    flex:0 0 auto;
    image-rendering:crisp-edges;
  }

  infinite-icons .row img:hover {
    background:#E6E6FA;
    outline:2px solid #E6E6FA;
  }

  infinite-icons .rowNumbers {
    position:absolute;
    left:calc(100% + .5em);
    top:calc(100% - 5pt);
    color:gray;
    font-size:10pt;
  }
`;

class InfiniteIcons extends HTMLElement {
  static ICON_W = 64;
  static ICON_H = 64;

  static MIN_SPACING = 16;
  static EXTRA_ROWS = 10;

  static defaultIconLoader = new IconLoader("iconCollection/");

  iconLoader = InfiniteIcons.defaultIconLoader;

  cols = 0;
  rowsPerPage = 0;

  evenPage = {element: null, pageNum: -1, generation: 0};
  oddPage =  {element: null, pageNum: -1, generation: 0};

  generation = 0;

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

  buildPage(page) {
    const fragment = document.createDocumentFragment();
    for (let r = 0; r < this.rowsPerPage; r++) {
      const row = document.createElement("div");
      row.className = "row";
      for (let c = 0; c < this.cols; c++) {
        const img = document.createElement("img");
        row.appendChild(img);
      }
      fragment.appendChild(row);
    }
    if (this.numbered) {
      const span = document.createElement("span");
      span.className = "rowNumbers";
      fragment.appendChild(span);
    }
    page.element.replaceChildren(fragment);
  }

  async fillPage(page) {
    const generation = ++page.generation;

    const pageRect = this.evenPage.element.getBoundingClientRect();
    const pageHeight = pageRect.bottom - pageRect.top;

    let startIndex = page.pageNum * this.rowsPerPage * this.cols;
    const total = this.rowsPerPage * this.cols;

    // Hide the icons while loading
    for (const row of page.element.children) {
      for (const img of row.children) {
        img.style.visibility = "hidden";
        img.src = '';
        img.title = '';
      }
    }

    // Move the page into position
    page.element.style.top = (page.pageNum * pageHeight) + "px";

    let iconIndex = 0;

    for await (const icon of this.iconLoader.loadIcons(startIndex, total)) {
      if (page.generation > generation) {
        // User must have scrolled
        console.log("User scrolled while previous icons were loading\n");
        return;
      }
      const rowIndex = Math.floor(iconIndex / this.cols);
      const colIndex = iconIndex % this.cols;

      const row = page.element.children[rowIndex];
      const img = row.children[colIndex];

      if (img) {
        img.style.visibility = "visible";
        img.src = icon.dataUrl;
        img.title = icon.title;
        img.dataset.srcFile = icon.name;
      }
      iconIndex++;
    }
    if (this.numbered) {
      page.element.lastElementChild.innerText = (startIndex + iconIndex).toLocaleString();
    }
  }

  async update(forceUpdate) {
    const pageRect   = this.evenPage.element.getBoundingClientRect();
    const pageHeight = pageRect.bottom - pageRect.top;

    const scrollY   = -this.container.getBoundingClientRect().top;
    const pageIndex = Math.max (0, Math.floor (scrollY/pageHeight));

    if (forceUpdate) {
      this.evenPageNum = this.oddPageNum = -1;
    }

    let newEvenPageNum, newOddPageNum;

    if (pageIndex % 2 == 0) {
      newEvenPageNum = pageIndex;
      newOddPageNum = pageIndex + 1;
    } else {
      newOddPageNum  = pageIndex;
      newEvenPageNum = pageIndex + 1;
    }

    if (this.evenPage.pageNum !== newEvenPageNum) {
      this.evenPage.pageNum = newEvenPageNum;
      await this.fillPage (this.evenPage);
    }

    if (this.oddPage.pageNum !== newOddPageNum ) {
      this.oddPage.pageNum = newOddPageNum;
      await this.fillPage (this.oddPage);
    }
  }

  clickOnImage(e) {
    if (e.target.matches('.page img')) {
      const src = e.target.dataset.srcFile;
      console.log(`Copied image ${src} to clipboard`);
      navigator.clipboard.writeText(src);
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
    style.textContent = infiniteIconsStyle;
    document.head.appendChild(style);
  }

  async layout() {
    this.inited = true;

    const totalIcons = await this.iconLoader.getTotalIcons();
    const minRowHeight = InfiniteIcons.ICON_H + InfiniteIcons.MIN_SPACING;

    const gridWidth = this.container.offsetWidth;
    const newCols = Math.max(1, Math.floor(gridWidth / (InfiniteIcons.ICON_W + InfiniteIcons.MIN_SPACING)));

    const rows = Math.ceil (totalIcons / newCols);

    this.container.style.minHeight = rows * minRowHeight + "px";

    const newRowsPerPage = Math.ceil(window.innerHeight / minRowHeight) + InfiniteIcons.EXTRA_ROWS;

    if ( (this.cols != newCols) || (this.rowsPerPage != newRowsPerPage) ) {
      console.log ("Rebuilding layout\n");
      this.cols        = newCols;
      this.rowsPerPage = newRowsPerPage;

      this.buildPage(this.evenPage);
      this.buildPage(this.oddPage);
    }

    this.update(true);
    this.updatePopulationCounts();
  }

  connectedCallback() {
    this.installStyle();

    this.style.display = "block";
    this.innerHTML = infiniteIconsTemplate;

    this.container = this;
    this.evenPage.element = this.querySelector(".evenPage");
    this.oddPage.element  = this.querySelector(".oddPage");

    this.onClick = this.clickOnImage.bind(this);
    this.onScroll = () => this.update();
    this.onResize = () => this.layout();

    this.evenPage.element.addEventListener("click", this.onClick);
    this.oddPage.element.addEventListener("click", this.onClick);

    window.addEventListener("scroll", this.onScroll);
    window.addEventListener("resize", this.onResize);

    if (typeof ResizeObserver !== 'undefined') {
      this.resizeObserver = new ResizeObserver(() => this.layout());
      this.resizeObserver.observe(this.container);
    } else {
      window.addEventListener ("resize", () => this.layout());
    }

    this.layout();
  }

  disconnectedCallback() {
    this.evenPage.element.removeEventListener("click", this.onClick);
    this.oddPage.element.removeEventListener("click", this.onClick);

    window.removeEventListener("scrollend", this.onScroll);
    window.removeEventListener("resize", this.onResize);

    if (this.resizeObserver) {
      this.resizeObserver.disconnect();
    }
  }

  // Attributes:

  static get observedAttributes() {
    return ["numbered","populateCount"];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (!this.inited) return;
    if (name === "numbered") {
      this.buildPage(this.evenPage);
      this.buildPage(this.oddPage);
      this.update(true);
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