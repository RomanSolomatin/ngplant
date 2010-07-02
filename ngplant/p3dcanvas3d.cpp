/***************************************************************************

 Copyright (C) 2006  Sergey Prokhorchuk

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***************************************************************************/

#include <wx/wx.h>

#include <ngput/p3dglext.h>

#include <wx/glcanvas.h>

#include <ngpcore/p3dmathrng.h>

#include <p3dcamera.h>
#include <p3dlog.h>
#include <p3dapp.h>

#include <p3dpobject.h>

#include <p3dcanvas3d.h>

BEGIN_EVENT_TABLE(P3DCanvas3D,wxGLCanvas)
 EVT_PAINT(P3DCanvas3D::OnPaint)
 EVT_ERASE_BACKGROUND(P3DCanvas3D::OnEraseBackground)
 EVT_SIZE(P3DCanvas3D::OnSize)
 EVT_MOTION(P3DCanvas3D::OnMouseMotion)
 EVT_MOUSEWHEEL(P3DCanvas3D::OnMouseWheel)
 EVT_KEY_DOWN(P3DCanvas3D::OnChar)
 EVT_ENTER_WINDOW(P3DCanvas3D::OnMouseEnter)
END_EVENT_TABLE()

#define BaseNearHalfWidth  (30.0f)
#define BaseDistance       (30.0f)

static int P3DCanvas3DGLAttrs[] =
 {
  WX_GL_DOUBLEBUFFER, 1,
  WX_GL_DEPTH_SIZE, 16,
  0,0
 };

                   P3DCanvas3D::P3DCanvas3D(wxWindow      *parent)
                    : wxGLCanvas(parent,wxID_ANY,wxDefaultPosition,wxSize(512,512),0,wxT("GLCanvas"),P3DCanvas3DGLAttrs),
                      viewport(0,0,320,240)
 {
  int                                               w,h;

  GLExtInited = false;

  GetClientSize(&w,&h);

  if (h > 0)
   {
    viewport.setSize(w,h);
   }

  m_x = -1;
  m_y = -1;

  camera.RotateCS(P3DMATH_DEG2RAD(20.0f),1.0f,0.0f,0.0f);

  ZoomFactor = 1.0f;

  OrthoCamera = true;
 }

                   P3DCanvas3D::~P3DCanvas3D
                                           ()
 {
 }

static GLfloat     P3DLight0Ambient[]  = { 0.0f, 0.0f, 0.0f, 1.0f };
static GLfloat     P3DLight0Diffuse[]  = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat     P3DLight0Specular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
static GLfloat     P3DLight0Position[] = { 0.0f, 0.0f, 1.0f, 0.0f };
static GLfloat     P3DGlobalAmbient[]  = { 0.4f, 0.4f, 0.4f, 1.0f };

void               P3DCanvas3D::OnPaint            (wxPaintEvent       &event)
 {
  wxPaintDC                                         dc(this);
  float                                             camera_matrix[16];
  unsigned char                                     R,G,B;

  SetCurrent();

  if (!GLExtInited)
   {
    P3DGLExtInit();

    P3DLogInfo(wxT("OpenGL information...\n"));

    if (GLEW_ARB_shader_objects)
     {
      P3DLogInfo(wxT("ARB_shader_objects found...\n"));
     }

    if (GLEW_ARB_vertex_shader)
     {
      P3DLogInfo(wxT("ARB_vertex_shader found...\n"));
     }

    if (GLEW_ARB_fragment_shader)
     {
      P3DLogInfo(wxT("ARB_fragment_shader found...\n"));
     }

    if ((!GLEW_ARB_shader_objects) ||
        (!GLEW_ARB_vertex_shader)  ||
        (!GLEW_ARB_fragment_shader))
     {
      wxGetApp().DisableShaders();

      P3DLogInfo(wxT("Hardware/driver lacks GLSL support - shaders disabled\n"));
     }

    GLExtInited = true;
   }

  glShadeModel(GL_SMOOTH);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_ALPHA_TEST);

  wxGetApp().GetBackgroundColor(&R,&G,&B);

  glClearColor((float)R / 255.0f,(float)G / 255.0f,(float)B / 255.0f,1.0f);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);

  glLightfv(GL_LIGHT0,GL_AMBIENT,P3DLight0Ambient);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,P3DLight0Diffuse);
  glLightfv(GL_LIGHT0,GL_SPECULAR,P3DLight0Specular);

  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,P3DGlobalAmbient);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glViewport(viewport.getX(),viewport.getY(),viewport.getWidth(),viewport.getHeight());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  if (OrthoCamera)
   {
    float ZoomedHalfWidth;

    ZoomedHalfWidth = BaseNearHalfWidth * ZoomFactor;

    glOrtho(-ZoomedHalfWidth,ZoomedHalfWidth,
            -ZoomedHalfWidth/viewport.getAspect(),ZoomedHalfWidth/viewport.getAspect(),
            1.0f,100.0f);

    camera.SetDistance(BaseDistance);
   }
  else
   {
    gluPerspective(45.0,viewport.getAspect(),1.0f,1000.0f);

    camera.SetDistance(BaseDistance * ZoomFactor);
   }

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glLightfv(GL_LIGHT0,GL_POSITION,P3DLight0Position);

  camera.GetTransformToCS(camera_matrix);
  glMultMatrixf(camera_matrix);

  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);

  glDisable(GL_TEXTURE_2D);

  if (wxGetApp().IsShadersEnabled())
   {
    glUseProgramObjectARB(0);
   }

  if (wxGetApp().IsGroundVisible())
   {
    wxGetApp().GetGroundColor(&R,&G,&B);

    glColor3f((float)R / 255.0f,(float)G / 255.0f,(float)B / 255.0f);

    glBegin(GL_QUADS);
    glNormal3f(0.0f,1.0f,0.0f);
    glVertex3f(-20.0f,0.0f,20.0f);
    glVertex3f( 20.0f,0.0f,20.0f);
    glVertex3f( 20.0f,0.0f,-20.0f);
    glVertex3f(-20.0f,0.0f,-20.0f);
    glEnd();
   }

  const P3DPlantObject    *PlantObject;

  PlantObject = wxGetApp().GetPlantObject();

  if (PlantObject != 0)
   {
    PlantObject->Render();
   }

  glFlush();

  SwapBuffers();
 }

void               P3DCanvas3D::OnEraseBackground  (wxEraseEvent       &event)
 {
 }

void               P3DCanvas3D::OnSize          (wxSizeEvent        &event)
 {
  int                                               w,h;

  GetClientSize(&w,&h);

  if (h > 0)
   {
    viewport.setSize(w,h);
   }

  Refresh();
  Update();
 }

void               P3DCanvas3D::OnMouseMotion   (wxMouseEvent       &event)
 {
  long             dx,dy;

  if ((m_x < 0) || (m_y < 0))
   {
    m_x = event.GetX(); m_y = event.GetY();

    return;
   }

  dx = event.GetX() - m_x;
  dy = event.GetY() - m_y;

  if ((dx != 0) || (dy != 0))
   {
    if (event.MiddleIsDown())
     {
      if (event.ShiftDown())
       {
        camera.CenterMoveCS(dx * BaseNearHalfWidth * ZoomFactor * 2.0f / viewport.getWidth(),
                            -dy * BaseNearHalfWidth * ZoomFactor * 2.0f / viewport.getAspect() / viewport.getHeight(),
                            0.0f);
       }
      else
       {
        const P3DCameraControlPrefs   *CameraControlPrefs;

        CameraControlPrefs = wxGetApp().GetCameraControlPrefs();

        if (dy != 0)
         {
          camera.RotateCS(dy * CameraControlPrefs->MouseRotYSens,1.0f,0.0f,0.0f);
         }

        if (dx != 0)
         {
          if (CameraControlPrefs->MouseRotYCS)
           {
            camera.RotateCS(dx * CameraControlPrefs->MouseRotXSens,0.0f,1.0f,0.0f);
           }
          else
           {
            camera.RotateWS(dx * CameraControlPrefs->MouseRotXSens,0.0f,1.0f,0.0f);
           }
         }
       }
     }

    m_x += dx; m_y += dy;
   }

  wxGetApp().InvalidateCamera();
  Refresh();
 }

void               P3DCanvas3D::OnMouseWheel    (wxMouseEvent       &event)
 {
  float WheelMove;

  WheelMove = ((float)event.GetWheelRotation()) / 120.0f;

  ZoomFactor *= WheelMove < 0.0f ? 4.0f / 3.0f : 0.75f;

  wxGetApp().InvalidateCamera();
  Refresh();
 }

void               P3DCanvas3D::OnChar          (wxKeyEvent         &event)
 {
  bool             CameraChanged;

  CameraChanged = true;

  if      ((event.GetKeyCode() == WXK_NUMPAD1) ||
           (event.GetKeyCode() == WXK_NUMPAD_END))
   {
    P3DQuaternionf           Rotation;

    camera.SetCenter(0.0f,0.0f,0.0f);

    if (event.ShiftDown())
     {
      Rotation.FromAxisAndAngle(0.0f,1.0f,0.0f,P3DMATH_PI);
      camera.SetDirection(Rotation.q[0],
                          Rotation.q[1],
                          Rotation.q[2],
                          Rotation.q[3]);
     }
    else
     {
      camera.SetDirection(0.0f,0.0f,0.0f,1.0f);
     }
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD7) ||
            (event.GetKeyCode() == WXK_NUMPAD_HOME))
   {
    P3DQuaternionf           Rotation;

    if (event.ShiftDown())
     {
      Rotation.FromAxisAndAngle(-1.0f,0.0f,0.0f,P3DMATH_PI / 2);
     }
    else
     {
      Rotation.FromAxisAndAngle(1.0f,0.0f,0.0f,P3DMATH_PI / 2);
     }

    camera.SetCenter(0.0f,0.0f,0.0f);
    camera.SetDirection(Rotation.q[0],
                        Rotation.q[1],
                        Rotation.q[2],
                        Rotation.q[3]);
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD3) ||
            (event.GetKeyCode() == WXK_NUMPAD_PAGEDOWN))
   {
    P3DQuaternionf           Rotation;

    if (event.ShiftDown())
     {
      Rotation.FromAxisAndAngle(0.0f,1.0f,0.0f,P3DMATH_PI / 2);
     }
    else
     {
      Rotation.FromAxisAndAngle(0.0f,-1.0f,0.0f,P3DMATH_PI / 2);
     }

    camera.SetCenter(0.0f,0.0f,0.0f);
    camera.SetDirection(Rotation.q[0],
                        Rotation.q[1],
                        Rotation.q[2],
                        Rotation.q[3]);
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD4) ||
            (event.GetKeyCode() == WXK_NUMPAD_LEFT))
   {
    camera.RotateWS(P3DMATH_PI / 12.0f,0.0f,1.0f,0.0f);
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD6) ||
            (event.GetKeyCode() == WXK_NUMPAD_RIGHT))
   {
    camera.RotateWS(P3DMATH_PI / 12.0f,0.0f,-1.0f,0.0f);
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD8) ||
            (event.GetKeyCode() == WXK_NUMPAD_UP))
   {
    camera.RotateCS(P3DMATH_PI / 12.0f,1.0f,0.0f,0.0f);
   }
  else if  ((event.GetKeyCode() == WXK_NUMPAD2) ||
            (event.GetKeyCode() == WXK_NUMPAD_DOWN))
   {
    camera.RotateCS(P3DMATH_PI / 12.0f,-1.0f,0.0f,0.0f);
   }
  else if (event.GetKeyCode() == WXK_NUMPAD_ADD)
   {
    ZoomFactor *= 0.75f;
   }
  else if (event.GetKeyCode() == WXK_NUMPAD_SUBTRACT)
   {
    ZoomFactor *= 4.0f / 3.0f;
   }
  else if ((event.GetKeyCode() == WXK_NUMPAD5) ||
           (event.GetKeyCode() == WXK_NUMPAD_BEGIN))
   {
    OrthoCamera = !OrthoCamera;
   }
  else
   {
    CameraChanged = false;

    event.Skip();
   }

  if (CameraChanged)
   {
    wxGetApp().InvalidateCamera();
    Refresh();
   }
 }

void               P3DCanvas3D::OnMouseEnter    (wxMouseEvent       &event)
 {
  SetFocus();
 }
