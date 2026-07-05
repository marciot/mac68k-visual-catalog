import { IconLoader } from './iconLoader.js';

const iconLoader = new IconLoader("iconCollection/");

const ICON_W = 64;
const ICON_H = 64;

const MIN_SPACING = 16;
const EXTRA_ROWS = 10;

let cols;
let rowHeight;
let rowsPerPage;
let totalIcons;
let pageHeight;

function loadIcon(n) {
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

function buildPage(page) {
  page.innerHTML = "";
  for (let r = 0; r < rowsPerPage; r++) {
    const row = document.createElement("div");
    row.className = "row";
    for (let c = 0; c < cols; c++) {
      const img = document.createElement("img");
      row.appendChild(img);
    }
    page.appendChild(row);
  }
  const span = document.createElement("span");
  span.className = "rowNumbers";
  page.appendChild(span);
}

async function fillPage(page, pageNum) {
  const pageRect = document.getElementById("evenPage").getBoundingClientRect();
  const pageHeight = pageRect.bottom - pageRect.top;

  let startIndex = pageNum * rowsPerPage * cols;
  const total = rowsPerPage * cols;

  // Hide the icons while loading
  for (const row of page.children) {
    for (const img of row.children) {
      img.visibility = "hidden";
      img.src = '';
      img.title = '';
    }
  }

  // Move the page into position
  page.style.top = (pageNum * pageHeight) + "px";

  let iconIndex = 0;

  for await (const icon of iconLoader.loadIcons(startIndex, total)) {
    const rowIndex = Math.floor(iconIndex / cols);
    const colIndex = iconIndex % cols;

    const row = page.children[rowIndex];
    const img = row.children[colIndex];

    if (img) {
      img.visibility = "visible";
      img.src = icon.dataUrl;
      img.title = icon.title;
      img.dataset.srcFile = icon.name;
    }

    iconIndex++;
  }
  page.lastElementChild.innerText = (startIndex + iconIndex).toLocaleString();
}

let evenPage = -1;
let oddPage = -1;

async function update(forceUpdate) {
  const pageRect   = document.getElementById("evenPage").getBoundingClientRect();
  const pageHeight = pageRect.bottom - pageRect.top;

  const scrollY   = -document.getElementById("scrollingDiv").getBoundingClientRect().top;
  const pageIndex = Math.max (0, Math.floor (scrollY/pageHeight));

  if (forceUpdate) {
    evenPage = oddPage = -1;
  }

  let newEvenPage, newOddPage;

  if (pageIndex % 2 == 0) {
    newEvenPage  = pageIndex;
    newOddPage   = pageIndex + 1;
  } else {
    newOddPage   = pageIndex;
    newEvenPage  = pageIndex + 1;
  }

  if (evenPage !== newEvenPage) {
    evenPage = newEvenPage;
    await fillPage (document.getElementById("evenPage"), evenPage);
  }

  if (oddPage !== newOddPage ) {
    oddPage = newOddPage;
    await fillPage (document.getElementById("oddPage"), oddPage);
  }
}

async function initialize() {
  const minRowHeight = ICON_H + MIN_SPACING;
  const container = document.getElementById("scrollingDiv");

  const gridWidth = container.offsetWidth;
  const newCols = Math.max(1, Math.floor(gridWidth / (ICON_W + MIN_SPACING)));

  const rows = Math.ceil (await iconLoader.getTotalIcons() / newCols);

  container.style.height = rows * minRowHeight + "px";

  const newRowsPerPage = Math.ceil(window.innerHeight / minRowHeight) + EXTRA_ROWS;
  pageHeight = rowsPerPage * minRowHeight;

  if ( (cols != newCols) || (rowsPerPage != newRowsPerPage) ) {
    console.log ("Rebuilding layout\n");
    cols        = newCols;
    rowsPerPage = newRowsPerPage;

    buildPage(document.getElementById("evenPage"));
    buildPage(document.getElementById("oddPage"));
  }

  update(true);

  const totalIcons = await iconLoader.getTotalIcons();
  document.getElementById("totalIcons").innerText =  totalIcons.toLocaleString();
}

function clickOnImage(e) {
  if (e.target.matches('.page img')) {
    const src = e.target.dataset.srcFile;
    console.log(`Copied image ${src} to clipboard`);
    navigator.clipboard.writeText(src);
  }
}

document.getElementById("evenPage").addEventListener("click", clickOnImage);
document.getElementById("oddPage").addEventListener("click", clickOnImage);

window.addEventListener ("scrollend", () => update());

if (typeof ResizeObserver !== 'undefined') {
  window.addEventListener ("resize", () => initialize());
} else {
  const container = document.getElementById("scrollingDiv");
  const resizeObserver = new ResizeObserver(() => initialize());
  resizeObserver.observe(container);
}

initialize();