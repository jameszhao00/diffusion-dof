I was doing some stuff for my game architecture final project, and came up with the following AA idea. 
It works with subpixel information, and in the current implementation is pretty expensive.
I didn't do a lot of research into this area, so think of it as a implementation demo if it's already been proposed.

The idea is to cache, per pixel, the triangle being shaded. Later, we selectively supersample pixel occlusion using an edge mask. 

Demo
Requires a DX11 card. Black spots are artifacts from small triangles (around the head and zooming out too much). 
The 'AA Viz' option visualizes the triangles in the (AA Viz X, AA Viz Y) pixel.
Use the DX option to simulate subpixel movement
Shader source in shaders/aa.hlsl

Outline
1. Render the scene and store the pixel's triangle verticies.
	a. Store 3 float2s in NDC space to 32 bit SNORM textures
		a. NDC space because it's easy to get in the PS, and later on we can do 2D triangle-point containment instead of 3D triangle-ray intersection tests
	b. Currently I store 3 float3s to R32G32B32A32_FLOAT textures (for debugging/laziness reasons)
	c. Currently I render with no MSAA.
2. Render a supersampling mask.
	a. Currently I just render an AA wireframe
3. In the AA pass, for each pixel that we wish to super sample...
	a. Collect every triangle from the neighboring pixels... for up to a total of 9 triangles
		- When the triangles aren't tiny these 9 triangles will cover the entire pixel
	b. Collect every depth, color from the neighboring pixels.
	c. For every sample point (16 in the demo)
		- Perform a 2d triangle-point containment test for every triangle
		- Find the closest triangle (using the neighboring depth values)
			- Pretty crude approximation
		- Store the color for that triangle (using the neighboring color values)
	d. Normalize by # of samples hit
		- There's a background triangle that covers the entire NDC cube, so mesh-background edges work
		- However, in the interior we can get gaps at places where small triangles, which aren't necessarily stored, exist

Pros
1. Fixed storage cost for arbitrary sampling density. 
2. Stores partial subpixel information
3. Works well for long edges

Cons
1. Heavy initial memory cost (storing 3 float2s (right now float3s) per pixel)
2. Extremely ALU intensive with point - tri containment tests
	- Currently I use some generic algorithm... hopefully better ones exist
3. Artifacts with small (subpixel to few pixel) triangles
4. Incorrect (though minor) blending

Improvements
1. Alternative triangle representation...
	a. We just need local triangle information relative to the pixel
2. Way cheaper 2d triangle - point containment tests
	a. Currently the major 'demo bottleneck'.