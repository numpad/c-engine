// service-worker.js

const cacheName = 'app-cache-v1';
const filesToCache = [
	'cengine.html',
	'cengine.js',
	'cengine.wasm',
	'cengine.data',
];

self.addEventListener('install', (event) => {
	event.waitUntil(
		caches.open(cacheName).then((cache) => {
			return cache.addAll(filesToCache);
		})
	);
});

self.addEventListener('fetch', (event) => {
	event.respondWith(
		caches.match(event.request).then((response) => {
			return response || fetch(event.request);
		})
	);
});

