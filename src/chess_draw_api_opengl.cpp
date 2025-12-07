#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_RECT_COUNT        100
#define MAX_RECT_VERTEX_COUNT MAX_RECT_COUNT * 4
#define MAX_RECT_INDEX_COUNT  MAX_RECT_COUNT * 6

#define MAX_TEXTURES 8
#define MAX_LIGHTS   4

#define ASCII_CHAR_COUNT 128
#define ASCII_CHAR_SPACE 32

struct Vertex3D
{
    Vec3 position;
    Vec2 uv;
    Vec4 color;
    f32  textureIndex;
};

enum
{
    DRAW_PASS_PICKING,
    DRAW_PASS_SHADOW,
    DRAW_PASS_RENDER
};

struct FontCharacter
{
    f32 advanceX;
    f32 advanceY;
    f32 width;
    f32 rows;
    f32 left;
    f32 top;
    f32 textureXOffset;
};

struct BatchBuffer
{
    u32       count;
    GLuint    VAO;
    GLuint    VBO;
    Vertex3D* vertexBuffer;
    Vertex3D* vertexBufferPtr;
};

struct RenderData
{
    BatchBuffer   batch2D;
    BatchBuffer   batch3D;
    GLuint        quadIBO;
    GLuint        batchProgram;
    Camera3D*     camera3D;
    Camera2D*     camera2D;
    u32           renderPass;
    GLuint        mousePickingProgram;
    GLuint        mousePickingFBO;
    GLuint        mousePickingColorTexture;
    GLuint        mousePickingDepthTexture;
    GLuint        fontAtlasTexture;
    Vec2U         fontAtlasDimension;
    FontCharacter fontChars[ASCII_CHAR_COUNT];
    Vec2U         viewportDimension;
    GLuint        textures[MAX_TEXTURES];
    Light         lights[MAX_LIGHTS];
    u32           lightCount;
    GLuint        shadowProgram;
    GLuint        shadowFBO;
    GLuint        shadowDepthTexture;
    u32           shadowMapWidth;
    u32           shadowMapHeight;
    Mat4x4        shadowLightMatrix;
    GLuint        pbrProgram;
    GLuint        equirecToCubemapProgram;
    GLuint        envCubemapTexture;
    GLuint        irradianceProgram;
    GLuint        irradianceMap;
    GLuint        brdfProgram;
    GLuint        brdfLUTTexture;
    GLuint        prefilterProgram;
    GLuint        prefilterMap;
};

chess_internal RenderData gRenderData;

chess_internal const char* pbrVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec3 aTangent;

out vec2 uv;
out vec3 worldPos;
out vec3 normal;
out vec4 fragPosLightSpace;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightMatrix;

void main()
{
	mat3 normalMatrix = transpose(inverse(mat3(model)));

	uv                = aUV;
	worldPos          = vec3(model * vec4(aPos, 1.0));
	normal            = normalMatrix * aNormal;
	fragPosLightSpace = lightMatrix * vec4(worldPos, 1.0);

	gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

chess_internal const char* pbrFragmentShader = R"(
#version 330 core

out vec4 FragColor;
in  vec2 uv;
in  vec3 worldPos;
in  vec3 normal;
in  vec4 fragPosLightSpace;

// Materials
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D armMap;

// Shadow mapping
uniform sampler2D shadowMap;

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;

// lights
uniform vec4 lightPositions[4];
uniform vec3 lightColors[4];
uniform int  lightCount;

uniform vec3 viewPos;

const float PI = 3.14159265359;

vec3 getNormalFromMap()
{
	vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;

	vec3 Q1  = dFdx(worldPos);
	vec3 Q2  = dFdy(worldPos);
	vec2 st1 = dFdx(uv);
	vec2 st2 = dFdy(uv);

	vec3 N   = normalize(normal);
	vec3 T   = normalize(Q1*st2.t - Q2*st1.t);
	vec3 B   = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom       = PI * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalc(vec3 lightDir, vec3 normal)
{
	float shadow = 0.0;

	// perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;

	float closestDepth = texture(shadowMap, projCoords.xy).r;
	float currentDepth = projCoords.z;

	// Shadow acne fixing
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	// PCF
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;

	return shadow;
}

void main()
{
	vec3 albedo     = pow(texture(albedoMap, uv).rgb, vec3(2.2));
	float metallic  = texture(armMap, uv).b;
	float roughness = texture(armMap, uv).g;
	float ao        = texture(armMap, uv).r;

	// input lighting data
	vec3 N = getNormalFromMap();
	vec3 V = normalize(viewPos - worldPos);
	vec3 R = reflect(-V, N);

	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallic);

	// reflectance equation
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < lightCount; ++i)
	{
		// Skip directional lights
		if (lightPositions[i].w == 0.0f)
		{
			continue;
		}

		// calculate per-light radiance
		vec3 lightPos     = lightPositions[i].xyz;
		vec3 L            = normalize(lightPos - worldPos);
		vec3 H            = normalize(V + L);
		float distance    = length(lightPos - worldPos);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance     = lightColors[i] * attenuation;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, roughness);
		float G   = GeometrySmith(N, V, L, roughness);
		vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3 numerator    = NDF * G * F; 
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3 specular = numerator / denominator;
        
		// kS is equal to Fresnel
		vec3 kS = F;
		// for energy conservation, the diffuse and specular light can't
		// be above 1.0 (unless the surface emits light); to preserve this
		// relationship the diffuse component (kD) should equal 1.0 - kS.
		vec3 kD = vec3(1.0) - kS;
		// multiply kD by the inverse metalness such that only non-metals 
		// have diffuse lighting, or a linear blend if partly metal (pure metals
		// have no diffuse light).
		kD *= 1.0 - metallic;

		// scale light by NdotL
		float NdotL = max(dot(N, L), 0.0);

		// add to outgoing radiance Lo
		// note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}
	
	// ambient lighting (we now use IBL as the ambient term)
	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;

	vec3 irradiance = texture(irradianceMap, N).rgb;
	vec3 diffuse    = irradiance * albedo;

	// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor          = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
	vec2 brdf                      = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 specular                  = prefilteredColor * (F * brdf.x + brdf.y);
	vec3 ambient                   = (kD * diffuse + specular) * ao;
	float shadow                   = ShadowCalc(-lightPositions[0].xyz, N);

	vec3 color = ambient + Lo * (1.0 - shadow);

	// HDR tonemapping
	color = color / (color + vec3(1.0));
	// gamma correct
	color = pow(color, vec3(1.0/2.2));

	FragColor = vec4(color, 1.0);
	}
)";

chess_internal const char* cubemapVertexShader = R"(
#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 worldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
	worldPos    = aPos;
	gl_Position = projection * view * vec4(worldPos, 1.0);
}
)";

chess_internal const char* equirectToCubemapFragmentShader = R"(
#version 330 core

out vec4 FragColor;
in  vec3 worldPos;

uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}

void main()
{
	vec2 uv = SampleSphericalMap(normalize(worldPos));
	vec3 color = texture(equirectangularMap, uv).rgb;

	FragColor = vec4(color, 1.0);
}
)";

chess_internal const char* irradianceFragmentShader = R"(
#version 330 core
out vec4 FragColor;
in  vec3 worldPos;

uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main()
{
	// The world vector acts as the normal of a tangent surface
	// from the origin, aligned to WorldPos. Given this normal, calculate all
	// incoming radiance of the environment. The result of this radiance
	// is the radiance of light coming from -Normal direction, which is what
	// we use in the PBR shader to sample irradiance.
	vec3 N = normalize(worldPos);

	vec3 irradiance = vec3(0.0);

	// tangent space calculation from origin point
	vec3 up    = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up         = normalize(cross(N, right));
       
	float sampleDelta = 0.025;
	float nrSamples   = 0.0;
	for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
	{
		for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
		{
			// spherical to cartesian (in tangent space)
			vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N; 

			irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.0 / float(nrSamples));

	FragColor = vec4(irradiance, 1.0);
}
)";

chess_internal const char* shadowVertexSource   = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec3 aTangent;

uniform mat4 lightMatrix;
uniform mat4 model;

void main()
{
	gl_Position = lightMatrix * model * vec4(aPos, 1.0);
}
)";
chess_internal const char* shadowFragmentSource = R"(
#version 330 core
void main() {}
)";

chess_internal const char* batchVertexShader   = R"(
#version 330 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in vec2  aUV;
layout(location = 2) in vec4  aColor;
layout(location = 3) in float aTextureIndex;

out vec4  color;
out vec2  uv;
flat out float textureIndex;

uniform mat4 viewProj;

void main()
{
	color        = aColor;
	uv           = aUV;
	textureIndex = aTextureIndex;
	gl_Position  = viewProj * vec4(aPos, 1.0);
}
)";
chess_internal const char* batchFragmentShader = R"(
#version 330 core
in   vec4  color;
in   vec2  uv;
flat in    float textureIndex;
out  vec4  FragColor;

uniform sampler2D textures[8];

void main()
{
	vec4 texColor = texture(textures[int(textureIndex)], uv);
	FragColor     = texColor * color;
}
)";

chess_internal const char* mousePickingVertexSource   = R"(
#version 330
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";
chess_internal const char* mousePickingFragmentSource = R"(
#version 330
uniform uint objectId;
out uint FragColor;

void main()
{
	FragColor = objectId;
}
)";

chess_internal const char* brdfVertexSource   = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;

out vec2 uv;

void main()
{
	uv = aUV;
	gl_Position = vec4(aPos, 1.0);
}
)";
chess_internal const char* brdfFragmentSource = R"(
#version 330 core
out vec2 FragColor;
in vec2 uv;

const float PI = 3.14159265359;
// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;

	float phi      = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space H vector to world-space sample vector
	vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}


float GeometrySchlickGGX(float NdotV, float roughness)
{
	// note that we use a different k for IBL
	float a = roughness;
	float k = (a * a) / 2.0;

	float nom   = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
	vec3 V;
	V.x = sqrt(1.0 - NdotV*NdotV);
	V.y = 0.0;
	V.z = NdotV;

	float A = 0.0;
	float B = 0.0;

	vec3 N = vec3(0.0, 0.0, 1.0);

	const uint SAMPLE_COUNT = 1024u;
	for(uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		// generates a sample vector that's biased towards the
		// preferred alignment direction (importance sampling).
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(L.z, 0.0);
		float NdotH = max(H.z, 0.0);
		float VdotH = max(dot(V, H), 0.0);

		if(NdotL > 0.0)
		{
			float G     = GeometrySmith(N, V, L, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc    = pow(1.0 - VdotH, 5.0);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);
	return vec2(A, B);
}

void main()
{
	vec2 integratedBRDF = IntegrateBRDF(uv.x, uv.y);
	FragColor = integratedBRDF;
}
)";

chess_internal const char* prefilteredFragmentSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 worldPos;

uniform samplerCube environmentMap;
uniform float       roughness;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / denom;
}

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;

	float phi      = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space H vector to world-space sample vector
	vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

void main()
{
	vec3 N = normalize(worldPos);

	// make the simplifying assumption that V equals R equals the normal
	vec3 R = N;
	vec3 V = R;

	const uint SAMPLE_COUNT = 1024u;
	vec3 prefilteredColor = vec3(0.0);
	float totalWeight = 0.0;

	for(uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		// generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H = ImportanceSampleGGX(Xi, N, roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
		if(NdotL > 0.0)
		{
			// sample from the environment's mip level based on roughness/pdf
			float D     = DistributionGGX(N, H, roughness);
			float NdotH = max(dot(N, H), 0.0);
			float HdotV = max(dot(H, V), 0.0);
			float pdf   = D * NdotH / (4.0 * HdotV) + 0.0001;

			float resolution = 512.0; // resolution of source cubemap (per face)
			float saTexel    = 4.0 * PI / (6.0 * resolution * resolution);
			float saSample   = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

			float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

			prefilteredColor += textureLod(environmentMap, L, mipLevel).rgb * NdotL;
			totalWeight      += NdotL;
		}
	}

	prefilteredColor = prefilteredColor / totalWeight;

	FragColor = vec4(prefilteredColor, 1.0);
}
)";

chess_internal void   BatchBufferCreate(BatchBuffer* batch, GLuint quadIBO);
chess_internal void   BatchBufferFlush(BatchBuffer* batch, Mat4x4* viewProj);
chess_internal void   Batch3DFlush();
chess_internal void   Batch2DFlush();
chess_internal void   Batch2DAddRect(Rect rect, Vec4 color, Texture* texture, Rect textureRect);
chess_internal void   FreeTypeInit();
chess_internal void   UpdateMousePickingFBO();
chess_internal u32    PushTexture(Texture* texture);
chess_internal void   BindActiveTextures(GLint samplerArrayLocation);
chess_internal GLuint ProgramBuild(const char* vertexSource, const char* fragmentSource);
chess_internal GLuint CompileShader(GLenum type, const char* src);

DRAW_INIT(DrawInitProcedure)
{
    CHESS_LOG("Renderer api: InitDrawing");
    gRenderData.viewportDimension = { windowWidth, windowHeight };

    // ----------------------------------------------------------------------------
    // Batch buffers
    u32 indices[MAX_RECT_INDEX_COUNT]{};
    u32 offset = 0;
    for (u32 i = 0; i < MAX_RECT_INDEX_COUNT; i += 6)
    {
        // First triangle
        indices[i + 0] = 0 + offset;
        indices[i + 1] = 1 + offset;
        indices[i + 2] = 2 + offset;
        // Second triangle
        indices[i + 3] = 2 + offset;
        indices[i + 4] = 3 + offset;
        indices[i + 5] = 0 + offset;

        offset += 4;
    }
    glGenBuffers(1, &gRenderData.quadIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    BatchBufferCreate(&gRenderData.batch2D, gRenderData.quadIBO);
    BatchBufferCreate(&gRenderData.batch3D, gRenderData.quadIBO);
    // ----------------------------------------------------------------------------

    FreeTypeInit();
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);

    // ----------------------------------------------------------------------------
    // Textures
    u32 whitePixel = 0xFFFFFFFF;
    glGenTextures(1, &gRenderData.textures[0]);
    glBindTexture(GL_TEXTURE_2D, gRenderData.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    for (GLuint textureIndex = 1; textureIndex < ARRAY_COUNT(gRenderData.textures); textureIndex++)
    {
        gRenderData.textures[textureIndex] = -1;
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Shadow mapping
    glGenFramebuffers(1, &gRenderData.shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.shadowFBO);

    gRenderData.shadowMapWidth  = 2048;
    gRenderData.shadowMapHeight = 2048;

    glGenTextures(1, &gRenderData.shadowDepthTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.shadowDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, gRenderData.shadowMapWidth, gRenderData.shadowMapHeight, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRenderData.shadowDepthTexture, 0);

    GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        CHESS_LOG("OpenGL Framebuffer error, status: 0x%x", framebufferStatus);
        CHESS_ASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // ----------------------------------------------------------------------------

    gRenderData.pbrProgram              = ProgramBuild(pbrVertexShader, pbrFragmentShader);
    gRenderData.equirecToCubemapProgram = ProgramBuild(cubemapVertexShader, equirectToCubemapFragmentShader);
    gRenderData.irradianceProgram       = ProgramBuild(cubemapVertexShader, irradianceFragmentShader);
    gRenderData.shadowProgram           = ProgramBuild(shadowVertexSource, shadowFragmentSource);
    gRenderData.batchProgram            = ProgramBuild(batchVertexShader, batchFragmentShader);
    gRenderData.mousePickingProgram     = ProgramBuild(mousePickingVertexSource, mousePickingFragmentSource);
    gRenderData.brdfProgram             = ProgramBuild(brdfVertexSource, brdfFragmentSource);
    gRenderData.prefilterProgram        = ProgramBuild(cubemapVertexShader, prefilteredFragmentSource);

    UpdateMousePickingFBO();

    glEnable(GL_MULTISAMPLE);
}

DRAW_DESTROY(DrawDestroyProcedure)
{
    delete[] gRenderData.batch2D.vertexBuffer;
    delete[] gRenderData.batch3D.vertexBuffer;
}

DRAW_BEGIN(DrawBeginProcedure)
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (gRenderData.viewportDimension != Vec2U{ windowWidth, windowHeight } && (windowWidth != 0 && windowHeight != 0))
    {
        gRenderData.viewportDimension = { windowWidth, windowHeight };
        UpdateMousePickingFBO();
    }

    gRenderData.batch3D.vertexBufferPtr = gRenderData.batch3D.vertexBuffer;
    gRenderData.batch3D.count           = 0;

    gRenderData.batch2D.vertexBufferPtr = gRenderData.batch2D.vertexBuffer;
    gRenderData.batch2D.count           = 0;

    for (u32 textureIndex = 1; textureIndex < ARRAY_COUNT(gRenderData.textures); textureIndex++)
    {
        gRenderData.textures[textureIndex] = -1;
    }
}

DRAW_END(DrawEndProcedure)
{
    Batch3DFlush();
    Batch2DFlush();
}

DRAW_BEGIN_3D(DrawBegin3D)
{
    CHESS_ASSERT(camera);
    gRenderData.camera3D = camera;
}

DRAW_END_3D(DrawEnd3D)
{
    //
}

DRAW_BEGIN_2D(DrawBegin2DProcedure)
{
    CHESS_ASSERT(camera);
    gRenderData.camera2D = camera;
}

DRAW_END_2D(DrawEnd2DProcedure)
{
    //
}

DRAW_PLANE_3D(DrawPlane3DProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    u32 planeVertexCount = gRenderData.batch3D.count * 4;
    if (planeVertexCount >= MAX_RECT_VERTEX_COUNT)
    {
        Batch3DFlush();
    }

    chess_internal constexpr Vec3 planeVertices[4] = {
        { 1.0f, 0.0f, 1.0f },   // top-right
        { -1.0f, 0.0f, 1.0f },  // top-left
        { -1.0f, 0.0f, -1.0f }, // bottom-left
        { 1.0f, 0.0f, -1.0f }   // bottom-right
    };

    for (u32 i = 0; i < 4; ++i)
    {
        // Transform local vertex to world space using the model matrix
        Vec4 localPos  = { planeVertices[i].x, planeVertices[i].y, planeVertices[i].z, 1.0f };
        Vec4 worldPos4 = model * localPos;
        Vec3 worldPos3 = { worldPos4.x, worldPos4.y, worldPos4.z };

        gRenderData.batch3D.vertexBufferPtr->position     = worldPos3;
        gRenderData.batch3D.vertexBufferPtr->color        = color;
        gRenderData.batch3D.vertexBufferPtr->textureIndex = 0.0f;
        gRenderData.batch3D.vertexBufferPtr++;
    }

    gRenderData.batch3D.count++;
}

DRAW_PLANE_TEXTURE_3D(DrawPlaneTexture3DProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    u32 planeVertexCount = gRenderData.batch3D.count * 4;
    if (planeVertexCount >= MAX_RECT_VERTEX_COUNT)
    {
        Batch3DFlush();
    }

    f32 textureIndex = PushTexture(&texture);

    Vec3 planeVertices[4] = {
        { 1.0f, 0.0f, 1.0f },   // top-right
        { -1.0f, 0.0f, 1.0f },  // top-left
        { -1.0f, 0.0f, -1.0f }, // bottom-left
        { 1.0f, 0.0f, -1.0f }   // bottom-right
    };

    f32 x = textureRect.x;
    f32 y = textureRect.y;
    f32 w = textureRect.x + textureRect.w;
    f32 h = textureRect.y + textureRect.h;

    textureRect.x = x / texture.width;
    textureRect.y = y / texture.height;
    textureRect.w = w / texture.width;
    textureRect.h = h / texture.height;

    Vec2 uvs[4] = {
        { textureRect.w, textureRect.h }, // bottom-right
        { textureRect.x, textureRect.h }, // bottom-left
        { textureRect.x, textureRect.y }, // top-left
        { textureRect.w, textureRect.y }  // top-right
    };

    for (u32 i = 0; i < 4; ++i)
    {
        // Transform local vertex to world space using the model matrix
        Vec4 localPos  = { planeVertices[i].x, planeVertices[i].y, planeVertices[i].z, 1.0f };
        Vec4 worldPos4 = model * localPos;
        Vec3 worldPos3 = { worldPos4.x, worldPos4.y, worldPos4.z };

        gRenderData.batch3D.vertexBufferPtr->position     = worldPos3;
        gRenderData.batch3D.vertexBufferPtr->color        = color;
        gRenderData.batch3D.vertexBufferPtr->uv           = uvs[i];
        gRenderData.batch3D.vertexBufferPtr->textureIndex = textureIndex;
        gRenderData.batch3D.vertexBufferPtr++;
    }

    gRenderData.batch3D.count++;
}

DRAW_LIGHT_ADD(DrawLightAddProcedure)
{
    CHESS_ASSERT(gRenderData.lightCount < MAX_LIGHTS);
    gRenderData.lights[gRenderData.lightCount++] = light;
}

DRAW_MESH(DrawMeshProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    if (gRenderData.renderPass == DRAW_PASS_RENDER)
    {
        GLint modelLoc       = glGetUniformLocation(gRenderData.pbrProgram, "model");
        GLint albedoMapLoc   = glGetUniformLocation(gRenderData.pbrProgram, "albedoMap");
        GLint normalMapLoc   = glGetUniformLocation(gRenderData.pbrProgram, "normalMap");
        GLint metallicMapLoc = glGetUniformLocation(gRenderData.pbrProgram, "armMap");

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, material.albedo.id);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, material.normalMap.id);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, material.armMap.id);

        glUniform1i(albedoMapLoc, 4);
        glUniform1i(normalMapLoc, 5);
        glUniform1i(metallicMapLoc, 6);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else if (gRenderData.renderPass == DRAW_PASS_PICKING)
    {
        GLint modelLoc    = glGetUniformLocation(gRenderData.mousePickingProgram, "model");
        GLint objectIdLoc = glGetUniformLocation(gRenderData.mousePickingProgram, "objectId");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);
        glUniform1ui(objectIdLoc, objectId + 1);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else if (gRenderData.renderPass == DRAW_PASS_SHADOW)
    {
        GLint modelLoc = glGetUniformLocation(gRenderData.shadowProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else
    {
        CHESS_ASSERT(0);
    }
}

DRAW_MESH_GPU_UPLOAD(DrawMeshGPUUploadProcedure)
{
    CHESS_ASSERT(mesh);

    glGenVertexArrays(1, &mesh->VAO);
    glGenBuffers(1, &mesh->VBO);
    glGenBuffers(1, &mesh->IBO);

    glBindVertexArray(mesh->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * mesh->vertexCount, vertexs, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * mesh->indicesCount, indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, tangent));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
}

DRAW_BEGIN_PASS_PICKING(DrawBeginPassPickingProcedure)
{
    gRenderData.renderPass = DRAW_PASS_PICKING;
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);
    glDisable(GL_DITHER);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gRenderData.mousePickingProgram);

    GLint viewLoc       = glGetUniformLocation(gRenderData.mousePickingProgram, "view");
    GLint projectionLoc = glGetUniformLocation(gRenderData.mousePickingProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &gRenderData.camera3D->view.e[0][0]);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &gRenderData.camera3D->projection.e[0][0]);
}

DRAW_END_PASS_PICKING(DrawEndPassPickingProcedure)
{
    gRenderData.renderPass = DRAW_PASS_RENDER;
    glEnable(GL_DITHER);
    glUseProgram(0);
}

DRAW_BEGIN_PASS_SHADOW(DrawBeginPassShadowProcedure)
{
    gRenderData.renderPass        = DRAW_PASS_SHADOW;
    gRenderData.shadowLightMatrix = lightProj * lightView;

    glViewport(0, 0, gRenderData.shadowMapWidth, gRenderData.shadowMapHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    glUseProgram(gRenderData.shadowProgram);

    GLint lightMatrixLoc = glGetUniformLocation(gRenderData.shadowProgram, "lightMatrix");
    glUniformMatrix4fv(lightMatrixLoc, 1, GL_FALSE, &gRenderData.shadowLightMatrix.e[0][0]);
}

DRAW_END_PASS_SHADOW(DrawEndPassShadowProcedure)
{
    glViewport(0, 0, (GLsizei)gRenderData.viewportDimension.w, (GLsizei)gRenderData.viewportDimension.h);
    glCullFace(GL_BACK);
    glUseProgram(0);
}

DRAW_BEGIN_PASS_RENDER(DrawBeginPassRenderProcedure)
{
    gRenderData.renderPass = DRAW_PASS_RENDER;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint program = gRenderData.pbrProgram;
    glUseProgram(program);

    GLint viewLoc          = glGetUniformLocation(program, "view");
    GLint projectionLoc    = glGetUniformLocation(program, "projection");
    GLint viewPosLoc       = glGetUniformLocation(program, "viewPos");
    GLint lightMatrixLoc   = glGetUniformLocation(program, "lightMatrix");
    GLint lightCountLoc    = glGetUniformLocation(program, "lightCount");
    GLint irradianceMapLoc = glGetUniformLocation(program, "irradianceMap");
    GLint prefilterMapLoc  = glGetUniformLocation(program, "prefilterMap");
    GLint brdfLUTLoc       = glGetUniformLocation(program, "brdfLUT");
    GLint shadowMapLoc     = glGetUniformLocation(program, "shadowMap");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.irradianceMap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.prefilterMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gRenderData.brdfLUTTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gRenderData.shadowDepthTexture);

    glUniform1i(irradianceMapLoc, 0);
    glUniform1i(prefilterMapLoc, 1);
    glUniform1i(brdfLUTLoc, 2);
    glUniform1i(shadowMapLoc, 3);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &gRenderData.camera3D->view.e[0][0]);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &gRenderData.camera3D->projection.e[0][0]);
    glUniformMatrix4fv(lightMatrixLoc, 1, GL_FALSE, &gRenderData.shadowLightMatrix.e[0][0]);
    glUniform3fv(viewPosLoc, 1, &gRenderData.camera3D->position.x);
    glUniform1i(lightCountLoc, gRenderData.lightCount);

    for (u32 i = 0; i < gRenderData.lightCount; i++)
    {
        char position[64];
        char color[64];
        sprintf(position, "lightPositions[%d]", i);
        sprintf(color, "lightColors[%d]", i);

        GLint lightPosLoc   = glGetUniformLocation(program, position);
        GLint lightColorLoc = glGetUniformLocation(program, color);

        glUniform4fv(lightPosLoc, 1, &gRenderData.lights[i].position.x);
        glUniform3fv(lightColorLoc, 1, &gRenderData.lights[i].color.x);
    }
}

DRAW_END_PASS_RENDER(DrawEndPassRenderProcedure) { glUseProgram(0); }

DRAW_GET_OBJECT_AT_PIXEL(DrawGetObjectAtPixelProcedure)
{
    u32 objectId;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gRenderData.mousePickingFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &objectId);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Not found
    if (objectId != 0)
    {
        return objectId - 1;
    }

    return -1;
}

DRAW_TEXT(DrawTextProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    for (size_t i = 0; i < strlen(text); i++)
    {
        FontCharacter fontChar = gRenderData.fontChars[text[i]];

        Rect rect;
        rect.x = x + fontChar.left;
        rect.y = y - fontChar.top;
        rect.w = fontChar.width;
        rect.h = fontChar.rows;

        Rect textureRect;
        textureRect.x = (f32)fontChar.textureXOffset / (f32)gRenderData.fontAtlasDimension.w;
        textureRect.y = 0.0f;
        textureRect.w = (f32)(fontChar.textureXOffset + fontChar.width) / (f32)gRenderData.fontAtlasDimension.w;
        textureRect.h = (f32)fontChar.rows / (f32)gRenderData.fontAtlasDimension.h;

        Texture texture;
        texture.id = gRenderData.fontAtlasTexture;

        Batch2DAddRect(rect, color, &texture, textureRect);

        x += fontChar.advanceX;
    }
}

DRAW_TEXT_GET_SIZE(DrawTextGetSizeProcedure)
{
    Vec2 size{ 0.0f };

    for (size_t i = 0; i < strlen(text); i++)
    {
        FontCharacter fontChar = gRenderData.fontChars[text[i]];
        size.w += fontChar.advanceX + fontChar.left;
        size.h = Max(size.h, fontChar.rows);
    }

    return size;
}

DRAW_RECT(DrawRectProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    Batch2DAddRect(rect, color, nullptr, Rect{});
}

DRAW_RECT_TEXTURE(DrawRectTextureProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    Mat4x4 viewProj = gRenderData.camera2D->projection;

    float x = textureRect.x;
    float y = textureRect.y;
    float w = textureRect.x + textureRect.w;
    float h = textureRect.y + textureRect.h;

    textureRect.x = x / texture.width;
    textureRect.y = y / texture.height;
    textureRect.w = w / texture.width;
    textureRect.h = h / texture.height;

    Batch2DAddRect(rect, tintColor, &texture, textureRect);
}

DRAW_TEXTURE_CREATE(DrawTextureCreateProcedure)
{
    CHESS_ASSERT(image->pixels);
    CHESS_ASSERT(image->isValid);

    Texture result;
    result.width  = image->width;
    result.height = image->height;

    GLenum format;
    GLenum internalFormat;
    GLenum pixelType;

    switch (image->pixelFormat)
    {
    case IMAGE_PIXEL_FORMAT_RED:
    {
        internalFormat = (image->pixelType == IMAGE_PIXEL_TYPE_F32) ? GL_R32F : GL_RED;
        format         = GL_RED;
        break;
    }
    case IMAGE_PIXEL_FORMAT_RGB:
    {
        internalFormat = (image->pixelType == IMAGE_PIXEL_TYPE_F32) ? GL_RGB16F : GL_RGB;
        format         = GL_RGB;
        break;
    }
    case IMAGE_PIXEL_FORMAT_RGBA:
    {
        internalFormat = (image->pixelType == IMAGE_PIXEL_TYPE_F32) ? GL_RGBA32F : GL_RGBA;
        format         = GL_RGBA;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    switch (image->pixelType)
    {
    case IMAGE_PIXEL_TYPE_U8:
    {
        pixelType = GL_UNSIGNED_BYTE;
        break;
    }
    case IMAGE_PIXEL_TYPE_F32:
    {
        pixelType = GL_FLOAT;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    glGenTextures(1, &result.id);
    glBindTexture(GL_TEXTURE_2D, result.id);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)image->width, (GLsizei)image->height, 0, format, pixelType,
                 (GLvoid*)image->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    return result;
}

DRAW_ENVIRONMENT_SET_HDR_MAP(DrawEnvironmentSetHDRMapProcedure)
{
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // Cube
    GLuint cubeVAO;
    GLuint cubeVBO;
    f32    vertices[] = {
        // back face
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
        -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,  // top-left
        // front face
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,   // top-right
        -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
        -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
        // left face
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
        -1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-left
        -1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-right
        // right face
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-left
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-right
        1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // top-right
        1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // bottom-right
        1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,   // top-left
        1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-left
        // bottom face
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,  // top-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,   // bottom-left
        -1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
        -1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, // top-right
        // top face
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,  // top-right
        1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom-right
        -1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top-left
        -1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f   // bottom-left
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    // Quad
    f32 quadVertices[] = {
        // positions        // uvs
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };
    GLuint quadVAO;
    GLuint quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Setup framebuffer for drawing the cubemap
    GLsizei cubemapWidth  = 512;
    GLsizei cubemapHeight = 512;

    GLuint captureFBO;
    GLuint captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cubemapWidth, cubemapHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Render to the cubemap texture
    glGenTextures(1, &gRenderData.envCubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.envCubemapTexture);
    for (u32 i = 0; i < 6; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, cubemapWidth, cubemapHeight, 0, GL_RGB, GL_FLOAT,
                     nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Setup projection and view matrices for capturing data onto the 6 cubemap face directions
    Mat4x4 projection = Perspective(DEGTORAD(90.0f), 1.0f, 0.1f, 10.0f);
    // clang-format off
	Mat4x4 views[6]   =
	{ 
        LookAt(Vec3{ 0.0f }, Vec3{ 1.0f,  0.0f, 0.0f }, Vec3{ 0.0f, -1.0f, 0.0f }),
        LookAt(Vec3{ 0.0f }, Vec3{ -1.0f, 0.0f, 0.0f }, Vec3{ 0.0f, -1.0f, 0.0f }),
        LookAt(Vec3{ 0.0f }, Vec3{ 0.0f,  1.0f, 0.0f }, Vec3{ 0.0f, 0.0f, 1.0f }),
        LookAt(Vec3{ 0.0f }, Vec3{ 0.0f, -1.0f, 0.0f }, Vec3{ 0.0f, 0.0f, -1.0f }),
        LookAt(Vec3{ 0.0f }, Vec3{ 0.0f,  0.0f, 1.0f }, Vec3{ 0.0f, -1.0f, 0.0f }),
        LookAt(Vec3{ 0.0f }, Vec3{ 0.0f, 0.0f, -1.0f }, Vec3{ 0.0f, -1.0f, 0.0f })
	};
    // clang-format on

    // Convert HDR equirectangular environment map to cubemap equivalent
    glUseProgram(gRenderData.equirecToCubemapProgram);
    GLint equirectangularMapLoc = glGetUniformLocation(gRenderData.equirecToCubemapProgram, "equirectangularMap");
    GLint viewLoc               = glGetUniformLocation(gRenderData.equirecToCubemapProgram, "view");
    GLint projLoc               = glGetUniformLocation(gRenderData.equirecToCubemapProgram, "projection");

    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection.e[0][0]);

    glUniform1i(equirectangularMapLoc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture.id);

    glViewport(0, 0, cubemapWidth, cubemapHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindVertexArray(cubeVAO);
    for (u32 i = 0; i < 6; i++)
    {
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &views[i].e[0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                               gRenderData.envCubemapTexture, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create an irradiance cubemap
    glGenTextures(1, &gRenderData.irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.irradianceMap);
    for (u32 i = 0; i < 6; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

    // Solve diffuse integral by convolution to create an irradiance cubemap
    {
        glUseProgram(gRenderData.irradianceProgram);
        GLint projLoc           = glGetUniformLocation(gRenderData.irradianceProgram, "projection");
        GLint viewLoc           = glGetUniformLocation(gRenderData.irradianceProgram, "view");
        GLint environmentMapLoc = glGetUniformLocation(gRenderData.irradianceProgram, "environmentMap");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.envCubemapTexture);
        glUniform1i(environmentMapLoc, 0);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection.e[0][0]);

        glViewport(0, 0, 32, 32);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (u32 i = 0; i < 6; i++)
        {
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &views[i].e[0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                   gRenderData.irradianceMap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Create a pre-filter cubemap
    glGenTextures(1, &gRenderData.prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.prefilterMap);
    for (u32 i = 0; i < 6; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Run a quasi monte-carlo simulation on the environment lighting to create a prefilter cubemap
    {
        glUseProgram(gRenderData.prefilterProgram);

        GLint environmentMapLoc = glGetUniformLocation(gRenderData.prefilterProgram, "environmentMap");
        GLint projLoc           = glGetUniformLocation(gRenderData.prefilterProgram, "projection");
        GLint viewLoc           = glGetUniformLocation(gRenderData.prefilterProgram, "view");
        GLint roughnessLoc      = glGetUniformLocation(gRenderData.prefilterProgram, "roughness");

        glUniform1i(environmentMapLoc, 0);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection.e[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, gRenderData.envCubemapTexture);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        u32 maxMipLevels = 5;
        for (u32 mip = 0; mip < maxMipLevels; mip++)
        {
            // reisze framebuffer according to mip-level size
            u32 mipWidth  = (u32)(128 * pow(0.5, mip));
            u32 mipHeight = (u32)(128 * pow(0.5, mip));
            glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
            glViewport(0, 0, mipWidth, mipHeight);

            f32 roughness = (float)mip / (float)(maxMipLevels - 1);
            glUniform1fv(roughnessLoc, 1, &roughness);
            for (u32 i = 0; i < 6; i++)
            {
                glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &views[i].e[0][0]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                       gRenderData.prefilterMap, mip);

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Generate a 2D LUT from the BRDF equations used
    {
        glGenTextures(1, &gRenderData.brdfLUTTexture);

        // pre-allocate enough memory for the LUT texture.
        glBindTexture(GL_TEXTURE_2D, gRenderData.brdfLUTTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
        // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gRenderData.brdfLUTTexture, 0);

        glViewport(0, 0, 512, 512);
        glUseProgram(gRenderData.brdfProgram);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei)gRenderData.viewportDimension.w, (GLsizei)gRenderData.viewportDimension.h);

    GLuint vaos[2] = { quadVAO, cubeVAO };
    GLuint vbos[2] = { quadVBO, cubeVBO };
    glDeleteVertexArrays(2, vaos);
    glDeleteBuffers(2, vbos);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
}

DRAW_VSYNC(DrawVsyncProcedure) { wglSwapIntervalEXT(enabled); }

DrawAPI DrawApiCreate()
{
    DrawAPI result;

    result.Init                 = DrawInitProcedure;
    result.Destroy              = DrawDestroyProcedure;
    result.Begin                = DrawBeginProcedure;
    result.End                  = DrawEndProcedure;
    result.Begin3D              = DrawBegin3D;
    result.End3D                = DrawEnd3D;
    result.Plane3D              = DrawPlane3DProcedure;
    result.PlaneTexture3D       = DrawPlaneTexture3DProcedure;
    result.Mesh                 = DrawMeshProcedure;
    result.MeshGPUUpload        = DrawMeshGPUUploadProcedure;
    result.GetObjectAtPixel     = DrawGetObjectAtPixelProcedure;
    result.Text                 = DrawTextProcedure;
    result.TextGetSize          = DrawTextGetSizeProcedure;
    result.Begin2D              = DrawBegin2DProcedure;
    result.End2D                = DrawEnd2DProcedure;
    result.Rect                 = DrawRectProcedure;
    result.RectTexture          = DrawRectTextureProcedure;
    result.TextureCreate        = DrawTextureCreateProcedure;
    result.LightAdd             = DrawLightAddProcedure;
    result.BeginPassPicking     = DrawBeginPassPickingProcedure;
    result.EndPassPicking       = DrawEndPassPickingProcedure;
    result.BeginPassShadow      = DrawBeginPassShadowProcedure;
    result.EndPassShadow        = DrawEndPassShadowProcedure;
    result.BeginPassRender      = DrawBeginPassRenderProcedure;
    result.EndPassRender        = DrawEndPassRenderProcedure;
    result.EnvironmentSetHDRMap = DrawEnvironmentSetHDRMapProcedure;
    result.Vsync                = DrawVsyncProcedure;

    return result;
}

chess_internal void UpdateMousePickingFBO()
{
    if (glIsFramebuffer(gRenderData.mousePickingFBO) == GL_TRUE)
    {
        CHESS_LOG("Updating mouse picking framebuffer...");

        GLuint textures[2] = { gRenderData.mousePickingColorTexture, gRenderData.mousePickingDepthTexture };
        glDeleteTextures(2, textures);
        glDeleteFramebuffers(1, &gRenderData.mousePickingFBO);
    }
    else
    {
        CHESS_LOG("Creating mouse picking framebuffer...");
    }

    u32 width  = gRenderData.viewportDimension.w;
    u32 height = gRenderData.viewportDimension.h;

    glGenFramebuffers(1, &gRenderData.mousePickingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);

    glGenTextures(1, &gRenderData.mousePickingColorTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gRenderData.mousePickingColorTexture,
                           0);

    glGenTextures(1, &gRenderData.mousePickingDepthTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture, 0);

    GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        CHESS_LOG("OpenGL Framebuffer error, status: 0x%x", framebufferStatus);
        CHESS_ASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

chess_internal void BatchBufferCreate(BatchBuffer* batch, GLuint quadIBO)
{
    CHESS_ASSERT(batch);

    glGenVertexArrays(1, &batch->VAO);
    glGenBuffers(1, &batch->VBO);

    glBindVertexArray(batch->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_RECT_VERTEX_COUNT * sizeof(Vertex3D), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, uv));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, color));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, textureIndex));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    batch->vertexBuffer    = new Vertex3D[MAX_RECT_VERTEX_COUNT];
    batch->vertexBufferPtr = batch->vertexBuffer;
}

chess_internal void BatchBufferFlush(BatchBuffer* batch, Mat4x4* viewProj)
{
    CHESS_ASSERT(batch);

    glUseProgram(gRenderData.batchProgram);

    u32   vertexCount     = batch->count * 4;
    u32   indexCount      = batch->count * 6;
    GLint samplerArrayLoc = glGetUniformLocation(gRenderData.batchProgram, "textures");
    GLint viewProjLoc     = glGetUniformLocation(gRenderData.batchProgram, "viewProj");

    BindActiveTextures(samplerArrayLoc);
    glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, &viewProj->e[0][0]);

    glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex3D), batch->vertexBuffer);
    glBindVertexArray(batch->VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

    batch->vertexBufferPtr = batch->vertexBuffer;
    batch->count           = 0;
}

chess_internal void Batch3DFlush()
{
    if (gRenderData.batch3D.count > 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        Mat4x4 viewProj = gRenderData.camera3D->projection * gRenderData.camera3D->view;
        BatchBufferFlush(&gRenderData.batch3D, &viewProj);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

chess_internal void Batch2DFlush()
{
    if (gRenderData.batch2D.count > 0)
    {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        BatchBufferFlush(&gRenderData.batch2D, &gRenderData.camera2D->projection);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}

chess_internal void Batch2DAddRect(Rect rect, Vec4 color, Texture* texture, Rect textureRect)
{
    u32 vertexCount = gRenderData.batch2D.count * 4;
    if (vertexCount >= MAX_RECT_VERTEX_COUNT)
    {
        Batch2DFlush();
    }

    f32 textureIndex = 0.0f;

    if (texture)
    {
        textureIndex = (f32)PushTexture(texture);
    }

    // Top-right
    gRenderData.batch2D.vertexBufferPtr->position     = { rect.x + rect.w, rect.y, 0.0f };
    gRenderData.batch2D.vertexBufferPtr->uv           = { textureRect.w, textureRect.y };
    gRenderData.batch2D.vertexBufferPtr->color        = color;
    gRenderData.batch2D.vertexBufferPtr->textureIndex = textureIndex;
    gRenderData.batch2D.vertexBufferPtr++;

    // Top-left
    gRenderData.batch2D.vertexBufferPtr->position     = { rect.x, rect.y, 0.0f };
    gRenderData.batch2D.vertexBufferPtr->uv           = { textureRect.x, textureRect.y };
    gRenderData.batch2D.vertexBufferPtr->color        = color;
    gRenderData.batch2D.vertexBufferPtr->textureIndex = textureIndex;
    gRenderData.batch2D.vertexBufferPtr++;

    // Bottom-left
    gRenderData.batch2D.vertexBufferPtr->position     = { rect.x, rect.y + rect.h, 0.0f };
    gRenderData.batch2D.vertexBufferPtr->uv           = { textureRect.x, textureRect.h };
    gRenderData.batch2D.vertexBufferPtr->color        = color;
    gRenderData.batch2D.vertexBufferPtr->textureIndex = textureIndex;
    gRenderData.batch2D.vertexBufferPtr++;

    // Bottom-right
    gRenderData.batch2D.vertexBufferPtr->position     = { rect.x + rect.w, rect.y + rect.h, 0.0f };
    gRenderData.batch2D.vertexBufferPtr->uv           = { textureRect.w, textureRect.h };
    gRenderData.batch2D.vertexBufferPtr->color        = color;
    gRenderData.batch2D.vertexBufferPtr->textureIndex = textureIndex;
    gRenderData.batch2D.vertexBufferPtr++;

    gRenderData.batch2D.count++;
}

chess_internal void FreeTypeInit()
{
    FT_Library library;
    FT_Face    face;

    if (FT_Init_FreeType(&library))
    {
        CHESS_LOG("[FreeType] error on FT_Init_FreeType");
        CHESS_ASSERT(0);
    }

    if (FT_New_Face(library, "../data/DroidSans.ttf", 0, &face))
    {
        CHESS_LOG("[FreeType] error loading font");
        CHESS_ASSERT(0);
    }

    FT_Set_Pixel_Sizes(face, 0, 24);

    for (u32 charIndex = ASCII_CHAR_SPACE; charIndex < ASCII_CHAR_COUNT; charIndex++)
    {
        if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
        {
            CHESS_LOG("[FreeType] failed to load glyph for ASCII: %d", charIndex);
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;

        gRenderData.fontAtlasDimension.w += glyph->bitmap.width;
        gRenderData.fontAtlasDimension.h = Max(gRenderData.fontAtlasDimension.h, glyph->bitmap.rows);
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &gRenderData.fontAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.fontAtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gRenderData.fontAtlasDimension.w, gRenderData.fontAtlasDimension.h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    s32 xOffset = 0;
    for (u32 charIndex = ASCII_CHAR_SPACE; charIndex < ASCII_CHAR_COUNT; charIndex++)
    {
        if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
        {
            continue;
        }

        FT_GlyphSlot glyph  = face->glyph;
        s32          width  = glyph->bitmap.width;
        s32          height = glyph->bitmap.rows;

        FontCharacter* c  = &gRenderData.fontChars[charIndex];
        c->advanceX       = glyph->advance.x >> 6;
        c->advanceY       = glyph->advance.y >> 6;
        c->width          = width;
        c->rows           = height;
        c->left           = glyph->bitmap_left;
        c->top            = glyph->bitmap_top;
        c->textureXOffset = xOffset;

        glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
        xOffset += width;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
}

chess_internal u32 PushTexture(Texture* texture)
{
    CHESS_ASSERT(texture);

    u32 textureIndex = 0.0f;

    for (u32 i = 0; i < ARRAY_COUNT(gRenderData.textures); i++)
    {
        if (gRenderData.textures[i] == texture->id)
        {
            textureIndex = i;
            break;
        }
        // Empty slot
        if (gRenderData.textures[i] == -1)
        {
            gRenderData.textures[i] = texture->id;
            textureIndex            = i;
            break;
        }
    }

    return textureIndex;
}

chess_internal void BindActiveTextures(GLint samplerArrayLocation)
{
    for (int i = 0; i < ARRAY_COUNT(gRenderData.textures); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        if (gRenderData.textures[i] != -1)
        {
            glBindTexture(GL_TEXTURE_2D, gRenderData.textures[i]);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, gRenderData.textures[0]);
        }
    }

    constexpr int textureUnits[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    glUniform1iv(samplerArrayLocation, ARRAY_COUNT(gRenderData.textures), textureUnits);
}

chess_internal GLuint ProgramBuild(const char* vertexSource, const char* fragmentSource)
{
    GLuint result = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    glAttachShader(result, fs);
    glAttachShader(result, vs);
    glLinkProgram(result);

    GLint ok;
    glGetProgramiv(result, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetProgramInfoLog(result, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("OpenGL linking program: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return result;
}

chess_internal GLuint CompileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetShaderInfoLog(shader, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("OpenGL compiling %s shader: '%s'", type == GL_FRAGMENT_SHADER ? "fragment" : "vertex", infoBuffer);
        CHESS_ASSERT(0);
    }

    return shader;
}