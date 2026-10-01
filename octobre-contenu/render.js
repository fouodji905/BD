// Exporte chaque diapositive du planning en PNG 1080 x 1350 (format portrait Instagram).
const { chromium } = require(process.env.PW || "playwright");
const path = require("path");
const fs = require("fs");

(async () => {
  const out = path.join(__dirname, "images");
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1080, height: 1350 } });
  await page.goto("file://" + path.join(__dirname, "index.html"));
  await page.addStyleTag({ path: path.join(__dirname, "fonts", "fonts.css") });
  await page.addStyleTag({ content: `
    #render { position: fixed; inset: 0; z-index: 99; width: 1080px; height: 1350px; }
    #render .slide { width: 1080px; height: 1350px; padding: 140px 110px; gap: 40px; }
    #render .big { font-size: 78px; line-height: 1.16; }
    #render .small { font-size: 40px; max-width: 26ch; }
    #render .ref { font-size: 30px; }
    #render .count { font-size: 26px; top: 40px; right: 40px; padding: 6px 18px; }
    #render .swipe { font-size: 28px; bottom: 48px; right: 56px; }
    #render .handle { position: absolute; bottom: 48px; left: 56px; font: 600 28px var(--font-body); opacity: .85; }
  `});
  const posts = await page.evaluate(() => POSTS);
  for (const p of posts) {
    const n = p.slides.length;
    for (let i = 0; i < n; i++) {
      await page.evaluate(({ p, i, n }) => {
        // Espaces insécables de la typographie française : « mot » ; : ! ?
        const esc = s => s.replace(/&/g, "&amp;").replace(/</g, "&lt;")
          .replace(/ ([;:!?»])/g, "\u202F$1").replace(/« /g, "«\u202F");
        const s = p.slides[i];
        let r = document.getElementById("render");
        if (!r) { r = document.createElement("div"); r.id = "render"; document.body.appendChild(r); }
        r.innerHTML = `<div class="slide ${p.bg}">
          ${n > 1 ? `<span class="count">${i + 1}/${n}</span>` : ""}
          ${s[2] ? `<span class="ref">${esc(s[2])}</span>` : ""}
          <div class="big">${esc(s[0])}</div>
          ${s[1] ? `<div class="small">${esc(s[1])}</div>` : ""}
          <span class="handle">@lumiere.du.jour</span>
          ${p.f === "carrousel" && i < n - 1 ? `<span class="swipe">Glisse →</span>` : ""}
        </div>`;
      }, { p, i, n });
      await page.evaluate(() => document.fonts.ready);
      const slug = p.title.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
      const dir = path.join(out, `${String(p.d).padStart(2, "0")}-octobre-${slug}`);
      fs.mkdirSync(dir, { recursive: true });
      await page.locator("#render").screenshot({ path: path.join(dir, `${i + 1}.png`) });
    }
  }
  await browser.close();
})();
