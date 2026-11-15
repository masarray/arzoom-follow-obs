import { chromium } from 'playwright-core';
import fs from 'node:fs';
const base = 'http://127.0.0.1:8765';
const out = 'site-qa-screenshots';
fs.mkdirSync(out, { recursive: true });
let failures = [];
const browser = await chromium.launch({ channel: 'chrome', headless: true, args: ['--no-sandbox'] });
for (const [width, height] of [[1920,1080], [1440,900], [768,1024], [390,844], [320,740]]) {
  const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor: 1, reducedMotion: 'reduce' });
  const errors = [];
  page.on('pageerror', err => errors.push(err.message));
  await page.goto(base + '/index.html', { waitUntil: 'networkidle' });
  await page.screenshot({ path: out + '/home-' + width + '.png', fullPage: true, animations: 'disabled' });
  const m = await page.evaluate(() => ({
    viewport: innerWidth,
    scrollWidth: document.documentElement.scrollWidth,
    h1: document.querySelectorAll('h1').length,
    chapters: document.querySelectorAll('.chapter').length,
    screenshots: [...document.querySelectorAll('img')].filter(i => !i.complete || !i.naturalWidth).map(i => i.getAttribute('src')),
    download: [...document.querySelectorAll('a')].filter(a => a.textContent.includes('Download')).map(a => a.href)
  }));
  if (m.scrollWidth > m.viewport + 1) failures.push(width + ': horizontal overflow ' + m.scrollWidth + '/' + m.viewport);
  if (m.h1 !== 1 || m.chapters !== 4) failures.push(width + ': wrong content hierarchy');
  if (m.screenshots.length) failures.push(width + ': broken images ' + m.screenshots.join(','));
  if (!m.download.some(url => url.endsWith('ArZoom-OBS-Setup-windows-x64.exe'))) failures.push(width + ': missing stable installer');
  if (width <= 768) {
    const toggle = page.locator('.menu-toggle');
    await toggle.click();
    if (await toggle.getAttribute('aria-expanded') !== 'true' || !await page.locator('#site-nav a').first().isVisible()) failures.push(width + ': mobile menu failed to open');
    await page.keyboard.press('Escape');
    if (await toggle.getAttribute('aria-expanded') !== 'false') failures.push(width + ': Escape failed to close menu');
  }
  if (errors.length) failures.push(width + ': JS errors ' + errors.join(' | '));
  console.log(width, JSON.stringify(m));
  await page.close();
}
for (const pageName of ['guide.html','troubleshooting.html','privacy.html']) {
  const page = await browser.newPage({ viewport: { width: 390, height: 844 }});
  const response = await page.goto(base + '/' + pageName, { waitUntil: 'networkidle' });
  if (!response?.ok()) failures.push(pageName + ': failed HTTP response');
  const m = await page.evaluate(() => ({ scrollWidth: document.documentElement.scrollWidth, width: innerWidth, nav: !!document.querySelector('#site-nav') }));
  if (m.scrollWidth > m.width + 1 || !m.nav) failures.push(pageName + ': overflow or broken nav');
  await page.screenshot({ path: out + '/' + pageName.replace('.html','') + '-390.png', fullPage: true });
  await page.close();
}
await browser.close();
if (failures.length) { console.error('SITE QA FAILURES:\n' + failures.join('\n')); process.exit(1); }
console.log('SITE QA PASS: all viewports, assets, menus, docs and stable-download links');