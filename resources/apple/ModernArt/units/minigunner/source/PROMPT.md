# Directional generation prompt

Run the following template once for each authored facing: `N`, `NE`, `E`, `SE`
and `S`. Supply the accepted neighbouring sheet as a visual consistency
reference. The builder mirrors `SE`, `E` and `NE` for the remaining directions.

```text
Use case: stylized-concept
Asset type: production pose master sheet for one HD RTS infantry facing
Primary request: Preserve the same original generic helmeted rifle soldier,
equipment, colors, proportions, painterly realism, scale and lighting as the
reference. Create exactly ten isolated full-body versions in a strict 5-column
by 2-row grid. In every pose the soldier's BODY faces world-space <DIRECTION>
under a fixed orthographic overhead RTS camera. Rotate the actual soldier around
the vertical body/world axis; never rotate or tilt the 2D image. Upright poses
keep the head above the torso and boots below the torso.
Pose order: top row is standing alert, walking left stride, walking
passing/neutral, walking right stride and controlled standing rifle fire.
Bottom row is prone aiming, crawl left, crawl neutral, crawl right and crouched
transition between standing and prone.
Scene/background: flat uniform #ff00ff chroma-key background; no floor or
shadows; equal cells, consistent scale and ground anchor, generous padding.
Style: polished hand-painted RTS sprite, crisp realistic detail and a readable
silhouette at tiny size. Charcoal, muted olive, warm gray and pale neutral
team-color panels; no magenta on the soldier.
Constraints: exactly 5 columns and 2 rows; same soldier and rifle throughout;
no missing/extra figures, overlap, grid lines, text, labels, logos, insignia,
blood, muzzle flash, smoke, scenery, shadows or watermark.
```

The non-negotiable rule for infantry is that the camera never rotates with the
character. This keeps every standing and walking facing visually upright.
