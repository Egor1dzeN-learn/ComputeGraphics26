"""Cornell box: CPU ray tracing with a rectangular ceiling light."""

import argparse
import os
from pathlib import Path

import numpy as np
from PIL import Image

EPS = 1e-5
LIGHT_Y = 1.99
LIGHT_HALF = 0.32


def normalize(v):
    """Делает длину векторов равной 1, сохраняя их направление."""
    return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), EPS)


def scene(origins, directions):
    """Находит ближайшую поверхность для каждого луча и возвращает расстояние, нормаль и цвет."""
    n = len(directions)

    distance = np.full(n, np.inf)
    normal = np.zeros((n, 3))
    color = np.zeros((n, 3))

    def update(t, valid, normals, rgb):
        """Записывает новое попадание, если оно ближе предыдущего."""
        hit = valid & (t > EPS) & (t < distance)
        distance[hit] = t[hit]
        normal[hit] = normals if np.ndim(normals) == 1 else normals[hit]
        color[hit] = rgb



    for axis, value, nrm, rgb in (
        (0, -1, (1, 0, 0), (0.72, 0.075, 0.06)),
        (0, 1, (-1, 0, 0), (0.08, 0.55, 0.13)),
        (1, 0, (0, 1, 0), (0.76, 0.74, 0.69)),
        (1, 2, (0, -1, 0), (0.76, 0.74, 0.69)),
        (2, 2, (0, 0, -1), (0.76, 0.74, 0.69)),
    ):
        denom = directions[:, axis]
        t = (value - origins[:, axis]) / np.where(np.abs(denom) > EPS, denom, np.nan)
        p: np.ndarray = origins + t[:, None] * directions

        valid: np.ndarray = ((p[:, 0] >= -1-EPS) & (p[:, 0] <= 1+EPS) &
                 (p[:, 1] >= -EPS) & (p[:, 1] <= 2+EPS) &
                 (p[:, 2] >= -EPS) & (p[:, 2] <= 2+EPS))
        update(t, valid, np.array(nrm), rgb)



    low = np.array([-0.67, 0.0, 0.95])
    high = np.array([-0.05, 1.12, 1.57])
    safe: np.ndarray = np.where(np.abs(directions) > EPS, directions, np.where(directions < 0, -EPS, EPS))
    a, b = (low - origins) / safe, (high - origins) / safe
    near = np.minimum(a, b)
    far = np.maximum(a, b)
    # Луч попадает в блок, если интервалы пересечения по трём осям перекрываются.
    t_near: np.ndarray = near.max(axis=1)
    t_far: np.ndarray = far.min(axis=1)
    outside = t_near > EPS
    t = np.where(outside, t_near, t_far)
    axes: np.ndarray = np.where(outside, near.argmax(axis=1), far.argmin(axis=1))
    normals = np.zeros((n, 3))
    normals[np.arange(n), axes] = np.where(outside, -1, 1) * np.sign(directions[np.arange(n), axes])
    update(t, t_far >= np.maximum(t_near, 0), normals, (0.73, 0.71, 0.65))

    center = np.array([0.39, 0.37, 0.65])
    oc = origins - center
    half_b = np.sum(oc * directions, axis=1)
    # Дискриминант уравнения определяет, пересекает ли луч шар.
    discriminant = half_b**2 - (np.sum(oc * oc, axis=1) - 0.37**2)
    root = np.sqrt(np.maximum(discriminant, 0))
    t0, t1 = -half_b - root, -half_b + root
    t = np.where(t0 > EPS, t0, t1)
    normals = normalize(origins + t[:, None] * directions - center)
    update(t, discriminant >= 0, normals, (0.72, 0.72, 0.75))
    return distance, normal, color


def render(width, height, samples, seed):
    """Строит изображение комнаты с освещением и мягкими тенями."""

    rng: np.random.Generator = np.random.default_rng(seed)
    x = (np.arange(width) + 0.5) / width * 2 - 1
    y = 1 - (np.arange(height) + 0.5) / height * 2
    xx, yy = np.meshgrid(x * width / height, y)
    camera = np.array([0.0, 1.0, -3.35])
    directions: np.ndarray = normalize(np.stack((xx * 0.39, yy * 0.39, np.ones_like(xx)), axis=-1).reshape(-1, 3))
    origins: np.ndarray = np.broadcast_to(camera, directions.shape)
    t, normals, colors = scene(origins, directions)
    hit = np.isfinite(t)
    result = np.zeros_like(colors)
    points: np.ndarray = origins[hit] + t[hit, None] * directions[hit]
    ns, albedo = normals[hit], colors[hit]
    # Фоновая подсветка приближённо учитывает непрямой свет.
    illumination = np.full(len(points), 0.11)
    for i in range(samples):
        light: np.ndarray = np.column_stack((rng.uniform(-LIGHT_HALF, LIGHT_HALF, len(points)),
                                 np.full(len(points), LIGHT_Y),
                                 rng.uniform(1-LIGHT_HALF, 1+LIGHT_HALF, len(points))))
        delta = light - points
        light_distance = np.linalg.norm(delta, axis=1)
        direction = delta / light_distance[:, None]
        cosine = np.maximum(np.sum(ns * direction, axis=1), 0)
        emitter_cosine = np.maximum(direction[:, 1], 0)
        # Немного сдвигаем начало теневого луча, чтобы он не пересёк свою поверхность.
        shadow_distance, _, _ = scene(points + ns * EPS * 10, direction)
        visible: np.ndarray = shadow_distance >= light_distance - EPS * 20
        # Усредняем вклад видимых точек светильника с учётом углов и расстояния.
        illumination += 3.2 * cosine * emitter_cosine * visible / np.maximum(light_distance**2, 0.05) / samples
        if (i + 1) % max(1, samples // 4) == 0:
            print(f'Light samples: {i + 1}/{samples}', flush=True)
    result[hit] = albedo * illumination[:, None]
    lamp = hit & (np.abs(normals[:, 1] + 1) < EPS)
    ceiling_points = origins[lamp] + t[lamp, None] * directions[lamp]
    lamp_indices: np.ndarray = np.flatnonzero(lamp)
    lamp_indices = lamp_indices[(np.abs(ceiling_points[:, 0]) <= LIGHT_HALF) &
                                (np.abs(ceiling_points[:, 2] - 1) <= LIGHT_HALF)]
    result[lamp_indices] = 1.0
    result = np.clip(result, 0, 1) ** (1 / 2.2)
    return Image.fromarray((result.reshape(height, width, 3) * 255 + 0.5).astype(np.uint8))


def main():
    """Читает параметры запуска, рисует комнату и сохраняет PNG."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width', type=int, default=600)
    parser.add_argument('--height', type=int, default=600)
    parser.add_argument('--samples', type=int, default=32, help='samples of the area light per pixel')
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--output', type=Path, default=Path(__file__).parent / 'cornell_box.png')
    parser.add_argument('--no-show', action='store_true')
    args: argparse.Namespace = parser.parse_args()
    if min(args.width, args.height, args.samples) <= 0:
        parser.error('width, height and samples must be positive')
    image: Image.Image = render(args.width, args.height, args.samples, args.seed)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    print(f'Saved: {args.output.resolve()}')
    if not args.no_show:
        if os.name == 'nt':
            os.startfile(args.output.resolve())
        else:
            image.show()


if __name__ == '__main__':
    main()
