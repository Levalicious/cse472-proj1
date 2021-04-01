#include "pch.h"
#include "CMyRaytraceRenderer.h"

void CMyRaytraceRenderer::SetWindow(CWnd* p_window)
{
    m_window = p_window;
}

bool CMyRaytraceRenderer::RendererStart()
{
	m_intersection.Initialize();

	m_mstack.clear();


	// We have to do all of the matrix work ourselves.
	// Set up the matrix stack.
	CGrTransform t;
	t.SetLookAt(Eye().X(), Eye().Y(), Eye().Z(),
		Center().X(), Center().Y(), Center().Z(),
		Up().X(), Up().Y(), Up().Z());

	m_mstack.push_back(t);

	m_light = CGrPoint(3, 3, 3);

	m_material = NULL;

	return true;
}

void CMyRaytraceRenderer::RendererMaterial(CGrMaterial* p_material)
{
	m_material = p_material;
}

void CMyRaytraceRenderer::RendererPushMatrix()
{
	m_mstack.push_back(m_mstack.back());
}

void CMyRaytraceRenderer::RendererPopMatrix()
{
	m_mstack.pop_back();
}

void CMyRaytraceRenderer::RendererRotate(double a, double x, double y, double z)
{
	CGrTransform r;
	r.SetRotate(a, CGrPoint(x, y, z));
	m_mstack.back() *= r;
}

void CMyRaytraceRenderer::RendererTranslate(double x, double y, double z)
{
	CGrTransform r;
	r.SetTranslate(x, y, z);
	m_mstack.back() *= r;
}

//
// Name : CMyRaytraceRenderer::RendererEndPolygon()
// Description : End definition of a polygon. The superclass has
// already collected the polygon information
//

void CMyRaytraceRenderer::RendererEndPolygon()
{
    const std::list<CGrPoint>& vertices = PolyVertices();
    const std::list<CGrPoint>& normals = PolyNormals();
    const std::list<CGrPoint>& tvertices = PolyTexVertices();

    // Allocate a new polygon in the ray intersection system
    m_intersection.PolygonBegin();
    m_intersection.Material(m_material);

    if (PolyTexture())
    {
        m_intersection.Texture(PolyTexture());
    }

    std::list<CGrPoint>::const_iterator normal = normals.begin();
    std::list<CGrPoint>::const_iterator tvertex = tvertices.begin();

    for (std::list<CGrPoint>::const_iterator i = vertices.begin(); i != vertices.end(); i++)
    {
        if (normal != normals.end())
        {
            m_intersection.Normal(m_mstack.back() * *normal);
            normal++;
        }

        if (tvertex != tvertices.end())
        {
            m_intersection.TexVertex(*tvertex);
            tvertex++;
        }

        m_intersection.Vertex(m_mstack.back() * *i);
    }

    m_intersection.PolygonEnd();
}


bool CMyRaytraceRenderer::RendererEnd()
{
	m_intersection.LoadingComplete();

	double ymin = -tan(ProjectionAngle() / 2 * GR_DTOR);
	double yhit = -ymin * 2;

	double xmin = ymin * ProjectionAspect();
	double xwid = -xmin * 2;

	// Extinction coefficient for fog
	double extCoeff = 0.00001;

	for (int r = 0; r < m_rayimageheight; r++)
	{
		for (int c = 0; c < m_rayimagewidth; c++)
		{
			double colorFog[3] = { 0.862, 0.859, 0.874 };
			double colorLight[3] = { 1, 1, 1};
			float colorTotal[3] = { 0, 0, 0 };

			double x = xmin + (c + 0.5) / m_rayimagewidth * xwid;
			double y = ymin + (r + 0.5) / m_rayimageheight * yhit;


			// Construct a Ray
			CRay ray(CGrPoint(0, 0, 0), Normalize3(CGrPoint(x, y, -1, 0)));

			double t;                                   // Will be distance to intersection
			CGrPoint intersect;                         // Will by x,y,z location of intersection
			const CRayIntersection::Object* nearest;    // Pointer to intersecting object
			if (m_intersection.Intersect(ray, 1e20, NULL, nearest, t, intersect))
			{
				// We hit something...
				// Determine information about the intersection
				CGrPoint N;
				CGrMaterial* material;
				CGrTexture* texture;
				CGrPoint texcoord;

				m_intersection.IntersectInfo(ray, nearest, t,
					N, material, texture, texcoord);

				if (material != NULL)
				{
					//calculate shading
					CGrPoint V = Normalize3(ray.Direction()) * -1;
					CGrPoint L = Normalize3(m_light - intersect);  //TODO: check if intersect is value to use here?
					CGrPoint H = Normalize3(V + L);

					double spec = pow(max(Dot3(N, H), 0.0), 2.0);
					double ambient = .1;

					//calculate fog
					double fogWeight = exp(-t * extCoeff);
					double fogWeightInv = 1.0 - fogWeight;

					//shadow rays
					for (int currLightInd = 0; currLightInd < LightCnt(); ++currLightInd) 
					{
						const CGrRenderer::Light currLight = GetLight(currLightInd);

						colorTotal[0] += currLight.m_ambient[0];
						colorTotal[1] += currLight.m_ambient[1];
						colorTotal[2] += currLight.m_ambient[2];

						CRay shadowRay(intersect, Normalize3(CGrPoint(currLight.m_pos.X(), currLight.m_pos.Y(), currLight.m_pos.Z(), 0)));
						const CRayIntersection::Object* shadowHit;
						if (!m_intersection.Intersect(shadowRay, 1e20, nearest, shadowHit, t, intersect))
						{
							colorTotal[0] += currLight.m_diffuse[0] * material->Diffuse(0);
							colorTotal[1] += currLight.m_diffuse[1] * material->Diffuse(1);
							colorTotal[2] += currLight.m_diffuse[2] * material->Diffuse(2);

							colorTotal[0] += currLight.m_specular[0] * material->Specular(0);
							colorTotal[1] += currLight.m_specular[1] * material->Specular(1);
							colorTotal[2] += currLight.m_specular[2] * material->Specular(2);
						}
					}

					colorTotal[0] /= LightCnt() * 3;
					colorTotal[1] /= LightCnt() * 3;
					colorTotal[2] /= LightCnt() * 3;
					

					m_rayimage[r][c * 3 + 0] = BYTE((colorTotal[0] * fogWeight + fogWeightInv * colorFog[0]) * 255);
					m_rayimage[r][c * 3 + 1] = BYTE((colorTotal[1] * fogWeight + fogWeightInv * colorFog[1]) * 255);
					m_rayimage[r][c * 3 + 2] = BYTE((colorTotal[2] * fogWeight + fogWeightInv * colorFog[2]) * 255);

					/*

					if (lightHits == 0) {
						m_rayimage[r][c * 3 + 0] = BYTE(((material->Diffuse(0) * fogWeight * spec + fogWeightInv * colorFog[0]) + ambient * colorLight[0]) * 255);
						m_rayimage[r][c * 3 + 1] = BYTE(((material->Diffuse(1) * fogWeight * spec + fogWeightInv * colorFog[1]) + ambient * colorLight[0]) * 255);
						m_rayimage[r][c * 3 + 2] = BYTE(((material->Diffuse(2) * fogWeight * spec + fogWeightInv * colorFog[2]) + ambient * colorLight[0]) * 255);
					}
					else {
						m_rayimage[r][c * 3 + 0] = BYTE(0);
						m_rayimage[r][c * 3 + 1] = BYTE(0);
						m_rayimage[r][c * 3 + 2] = BYTE(0);
					}
					*/
					
				}



			}
			else
			{
				// We hit nothing...
				m_rayimage[r][c * 3] = colorFog[0] * 255;
				m_rayimage[r][c * 3 + 1] = colorFog[1] * 255;
				m_rayimage[r][c * 3 + 2] = colorFog[2] * 255;
			}
		}
		if ((r % 50) == 0)
		{
			m_window->Invalidate();
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				DispatchMessage(&msg);
		}
	}


	return true;
}
