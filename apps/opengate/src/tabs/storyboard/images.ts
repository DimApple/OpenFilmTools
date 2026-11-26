import { blobToImage } from '../../store/media';

const MAX = 1920;

/** Downscales large uploads (keeps IndexedDB and project files small); small images pass through. */
export async function prepareImage(file: Blob): Promise<Blob> {
  if (!file.type.startsWith('image/')) throw new Error('That file is not an image.');
  if (file.type === 'image/svg+xml' || file.type === 'image/gif') return file;
  const img = await blobToImage(file);
  if (img.width <= MAX && img.height <= MAX && file.size < 1.5e6) return file;
  const s = Math.min(1, MAX / Math.max(img.width, img.height));
  const c = document.createElement('canvas');
  c.width = Math.round(img.width * s);
  c.height = Math.round(img.height * s);
  c.getContext('2d')!.drawImage(img, 0, 0, c.width, c.height);
  const out = await new Promise<Blob | null>((r) => c.toBlob(r, 'image/jpeg', 0.88));
  return out ?? file;
}

export async function fetchStoryboardImage(input: string): Promise<Blob> {
  let url: URL;
  try {
    url = new URL(input.trim());
    if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) throw new Error();
  } catch {
    throw new Error('Enter a direct HTTP or HTTPS image link.');
  }
  let response: Response;
  try {
    response = await fetch(url, {
      mode: 'cors', credentials: 'omit', referrerPolicy: 'no-referrer',
      signal: AbortSignal.timeout(15000),
    });
  } catch {
    throw new Error('Could not download the image. Save it locally and upload the file if the site blocks copying.');
  }
  if (!response.ok) throw new Error('The image link could not be downloaded.');
  const image = await response.blob();
  if (!/^image\/(?:png|jpeg|webp|gif|svg\+xml)$/i.test(image.type)) {
    throw new Error('Use a direct link to a PNG, JPEG, WebP, GIF, or SVG image.');
  }
  return image;
}
