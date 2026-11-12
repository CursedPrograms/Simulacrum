// Terminus's page: the feature cards and the year
(function () {
  'use strict';
  const features = [
    ['Three steps', 'Add your photos, pick a quality, press one button. It finds where each photo was taken from, measures depth and builds a textured model.'],
    ['Watch it happen', 'The steps tick off with a progress bar; the matched points and the cameras that took the photos appear as soon as they\'re placed.'],
    ['Every setting a slider', 'Feature detail, matching, depth photo size, surface detail, smoothing, triangles kept, texture size and sharpness, hole filling.'],
    ['Turn it round', 'Textured, clay or wireframe; the cameras, a floor and the light; drag to turn, wheel to zoom; save a picture of it.'],
    ['Save it anywhere', 'GLB with the texture inside for any 3D app or the web, OBJ with its textures, or STL for 3D printing, at the real size you set.'],
    ['Any PC', 'An NVIDIA card measures depth for full detail; without one, Terminus builds the model from the matched points. Everything it needs is inside.']
  ];
  const grid = document.getElementById('features');
  if (grid) {
    for (const [title, text] of features) {
      const card = document.createElement('article');
      const h = document.createElement('h2');
      h.textContent = title;
      const p = document.createElement('p');
      p.textContent = text;
      card.append(h, p);
      grid.appendChild(card);
    }
  }
  const year = document.getElementById('year');
  if (year) year.textContent = String(Math.max(2026, new Date().getFullYear()));
})();
