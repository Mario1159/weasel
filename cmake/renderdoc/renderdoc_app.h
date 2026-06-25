/* ***************************************************************************
 * renderdoc_app.h - RenderDoc in-application API 1.7.0
 * Vendored from:
 * https://github.com/baldurk/renderdoc/blob/v1.x/renderdoc/app/renderdoc_app.h
 * Re-vendor manually when upgrading RenderDoc:
 *   curl -L
 * https://raw.githubusercontent.com/baldurk/renderdoc/v1.7.0/renderdoc/app/renderdoc_app.h
 * \ -o cmake/renderdoc/renderdoc_app.h No link-time dependency: we resolve
 * RENDERDOC_GetAPI via dlopen/GetModuleHandle at runtime; do not add
 * librenderdoc to the link line.
 * **************************************************************************/
#ifndef RENDERDOC_APP_H
#define RENDERDOC_APP_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Versioning                                                          */
/* ------------------------------------------------------------------ */

typedef enum RENDERDOC_Version
{
  eRENDERDOC_API_Version_1_0_0 = 10000,
  eRENDERDOC_API_Version_1_0_1 = 10001,
  eRENDERDOC_API_Version_1_0_2 = 10002,
  eRENDERDOC_API_Version_1_1_0 = 10100,
  eRENDERDOC_API_Version_1_1_1 = 10101,
  eRENDERDOC_API_Version_1_1_2 = 10102,
  eRENDERDOC_API_Version_1_2_0 = 10200,
  eRENDERDOC_API_Version_1_3_0 = 10300,
  eRENDERDOC_API_Version_1_4_0 = 10400,
  eRENDERDOC_API_Version_1_4_1 = 10401,
  eRENDERDOC_API_Version_1_4_2 = 10402,
  eRENDERDOC_API_Version_1_5_0 = 10500,
  eRENDERDOC_API_Version_1_6_0 = 10600,
  eRENDERDOC_API_Version_1_7_0 = 10700,
} RENDERDOC_Version;

typedef int (*pRENDERDOC_GetAPI) (RENDERDOC_Version version,
                                  void **outAPIPointers);

/* ------------------------------------------------------------------ */
/* Capture options                                                     */
/* ------------------------------------------------------------------ */

typedef enum RENDERDOC_CaptureOption
{
  eRENDERDOC_Option_AllowVSync = 0,
  eRENDERDOC_Option_AllowFullscreen = 1,
  eRENDERDOC_Option_APIValidation = 2,
  eRENDERDOC_Option_CaptureCallstacks = 3,
  eRENDERDOC_Option_CaptureCallstacksOnlyActions = 4,
  eRENDERDOC_Option_DelayForDebugger = 5,
  eRENDERDOC_Option_VerifyBufferWrites = 6,
  eRENDERDOC_Option_HookIntoChildren = 7,
  eRENDERDOC_Option_RefAllResources = 8,
  eRENDERDOC_Option_CaptureAllCmdLists = 9,
  eRENDERDOC_Option_DebugOutputMute = 10,
  eRENDERDOC_Option_AllowUnsupportedCPUVendor = 11,
} RENDERDOC_CaptureOption;

/* ------------------------------------------------------------------ */
/* Input buttons                                                       */
/* ------------------------------------------------------------------ */

typedef enum RENDERDOC_InputButton
{
  eRENDERDOC_Key_0 = 0x30,
  eRENDERDOC_Key_1 = 0x31,
  eRENDERDOC_Key_2 = 0x32,
  eRENDERDOC_Key_3 = 0x33,
  eRENDERDOC_Key_4 = 0x34,
  eRENDERDOC_Key_5 = 0x35,
  eRENDERDOC_Key_6 = 0x36,
  eRENDERDOC_Key_7 = 0x37,
  eRENDERDOC_Key_8 = 0x38,
  eRENDERDOC_Key_9 = 0x39,

  eRENDERDOC_Key_A = 0x41,
  eRENDERDOC_Key_B = 0x42,
  eRENDERDOC_Key_C = 0x43,
  eRENDERDOC_Key_D = 0x44,
  eRENDERDOC_Key_E = 0x45,
  eRENDERDOC_Key_F = 0x46,
  eRENDERDOC_Key_G = 0x47,
  eRENDERDOC_Key_H = 0x48,
  eRENDERDOC_Key_I = 0x49,
  eRENDERDOC_Key_J = 0x4A,
  eRENDERDOC_Key_K = 0x4B,
  eRENDERDOC_Key_L = 0x4C,
  eRENDERDOC_Key_M = 0x4D,
  eRENDERDOC_Key_N = 0x4E,
  eRENDERDOC_Key_O = 0x4F,
  eRENDERDOC_Key_P = 0x50,
  eRENDERDOC_Key_Q = 0x51,
  eRENDERDOC_Key_R = 0x52,
  eRENDERDOC_Key_S = 0x53,
  eRENDERDOC_Key_T = 0x54,
  eRENDERDOC_Key_U = 0x55,
  eRENDERDOC_Key_V = 0x56,
  eRENDERDOC_Key_W = 0x57,
  eRENDERDOC_Key_X = 0x58,
  eRENDERDOC_Key_Y = 0x59,
  eRENDERDOC_Key_Z = 0x5A,

  eRENDERDOC_Key_NonPrintable = 0x100,
  eRENDERDOC_Key_Divide = 0x101,
  eRENDERDOC_Key_Multiply = 0x102,
  eRENDERDOC_Key_Subtract = 0x103,
  eRENDERDOC_Key_Plus = 0x104,

  eRENDERDOC_Key_F1 = 0x105,
  eRENDERDOC_Key_F2 = 0x106,
  eRENDERDOC_Key_F3 = 0x107,
  eRENDERDOC_Key_F4 = 0x108,
  eRENDERDOC_Key_F5 = 0x109,
  eRENDERDOC_Key_F6 = 0x10A,
  eRENDERDOC_Key_F7 = 0x10B,
  eRENDERDOC_Key_F8 = 0x10C,
  eRENDERDOC_Key_F9 = 0x10D,
  eRENDERDOC_Key_F10 = 0x10E,
  eRENDERDOC_Key_F11 = 0x10F,
  eRENDERDOC_Key_F12 = 0x110,

  eRENDERDOC_Key_Home = 0x111,
  eRENDERDOC_Key_End = 0x112,
  eRENDERDOC_Key_Insert = 0x113,
  eRENDERDOC_Key_Delete = 0x114,
  eRENDERDOC_Key_PageUp = 0x115,
  eRENDERDOC_Key_PageDn = 0x116,

  eRENDERDOC_Key_Backspace = 0x117,
  eRENDERDOC_Key_Tab = 0x118,
  eRENDERDOC_Key_PrtScrn = 0x119,
  eRENDERDOC_Key_Pause = 0x11A,
} RENDERDOC_InputButton;

/* ------------------------------------------------------------------ */
/* Overlay bitmask                                                     */
/* ------------------------------------------------------------------ */

typedef enum RENDERDOC_OverlayBits
{
  eRENDERDOC_Overlay_Enabled = 0x1,
  eRENDERDOC_Overlay_FrameRate = 0x2,
  eRENDERDOC_Overlay_FrameNumber = 0x4,
  eRENDERDOC_Overlay_CaptureList = 0x8,
  eRENDERDOC_Overlay_Default = 0xF,
  eRENDERDOC_Overlay_All = ~0U,
  eRENDERDOC_Overlay_None = 0,
} RENDERDOC_OverlayBits;

/* ------------------------------------------------------------------ */
/* Annotation API (added in 1.7.0)                                     */
/* ------------------------------------------------------------------ */

typedef enum RENDERDOC_AnnotationType
{
  eRENDERDOC_Empty = 0,
  eRENDERDOC_Bool = 1,
  eRENDERDOC_Int32 = 2,
  eRENDERDOC_UInt32 = 3,
  eRENDERDOC_Int64 = 4,
  eRENDERDOC_UInt64 = 5,
  eRENDERDOC_Float = 6,
  eRENDERDOC_Double = 7,
  eRENDERDOC_String = 8,
  eRENDERDOC_APIObject = 9,
} RENDERDOC_AnnotationType;

typedef struct RENDERDOC_ResourceId
{
  uint64_t id;
} RENDERDOC_ResourceId;

typedef struct RENDERDOC_AnnotationValue
{
  /* Use the union member matching the RENDERDOC_AnnotationType. */
  int32_t i32;
  uint32_t u32;
  int64_t i64;
  uint64_t u64;
  float f32;
  double f64;
  int boolean;
  const char *str;
  RENDERDOC_ResourceId apiObject;
} RENDERDOC_AnnotationValue;

typedef struct RENDERDOC_AnnotationVectorValue
{
  /* Up to 4 components matching valueVectorWidth. */
  float f32v[4];
  double f64v[4];
  uint32_t u32v[4];
  int32_t i32v[4];
  uint64_t u64v[4];
  int64_t i64v[4];
  int boolv[4];
} RENDERDOC_AnnotationVectorValue;

/* OpenGL has no real handles; the user passes a pointer to this so
 * RenderDoc can match the (object type, name) pair. */
typedef struct RENDERDOC_GLResourceReference
{
  uint32_t objectType;
  void *resource;
} RENDERDOC_GLResourceReference;

/* ------------------------------------------------------------------ */
/* Platform handles                                                    */
/* ------------------------------------------------------------------ */

typedef void *RENDERDOC_DevicePointer;
typedef void *RENDERDOC_WindowHandle;

/* For Vulkan the device pointer is the dispatch table inside the
 * VkInstance, not the VkInstance itself. The macro below pulls the
 * right value out of any VkInstance. */
#if defined(__cplusplus)
inline RENDERDOC_DevicePointer
RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE (void *instance)
{
  /* The dispatch table is the first pointer stored in the loader's
   * VkInstance trampoline. Cast through uintptr_t to silence strict
   * aliasing warnings. */
  return *reinterpret_cast<RENDERDOC_DevicePointer *> (instance);
}
#endif

/* ------------------------------------------------------------------ */
/* API structs                                                         */
/* Each version is a strict superset of the previous one.              */
/* ------------------------------------------------------------------ */

typedef struct RENDERDOC_API_1_7_0
{
  /* ---- 1.0.0 ---- */
  int (*GetAPIVersion) (int *major, int *minor, int *patch);
  int (*SetCaptureOptionU32) (RENDERDOC_CaptureOption opt, uint32_t val);
  int (*SetCaptureOptionF32) (RENDERDOC_CaptureOption opt, float val);
  uint32_t (*GetCaptureOptionU32) (RENDERDOC_CaptureOption opt);
  float (*GetCaptureOptionF32) (RENDERDOC_CaptureOption opt);

  void (*SetFocusToggleKeys) (RENDERDOC_InputButton *keys, int num);
  void (*SetCaptureKeys) (RENDERDOC_InputButton *keys, int num);

  uint32_t (*GetOverlayBits) (void);
  void (*MaskOverlayBits) (uint32_t And, uint32_t Or);

  void (*RemoveHooks) (void);
  void (*UnloadCrashHandler) (void);

  void (*SetCaptureFilePathTemplate) (const char *pathtemplate);
  const char *(*GetCaptureFilePathTemplate) (void);

  uint32_t (*GetNumCaptures) (void);
  uint32_t (*GetCapture) (uint32_t idx, char *filename, uint32_t *pathlength,
                          uint64_t *timestamp);

  void (*TriggerCapture) (void);

  uint32_t (*IsTargetControlConnected) (void);
  uint32_t (*LaunchReplayUI) (uint32_t connectTargetControl,
                              const char *cmdline);

  void (*SetActiveWindow) (RENDERDOC_DevicePointer device,
                           RENDERDOC_WindowHandle wndHandle);

  void (*StartFrameCapture) (RENDERDOC_DevicePointer device,
                             RENDERDOC_WindowHandle wndHandle);
  uint32_t (*IsFrameCapturing) (void);
  uint32_t (*EndFrameCapture) (RENDERDOC_DevicePointer device,
                               RENDERDOC_WindowHandle wndHandle);

  /* ---- 1.1.0 ---- */
  void (*TriggerMultiFrameCapture) (uint32_t numFrames);

  /* ---- 1.2.0 ---- */
  void (*SetCaptureFileComments) (const char *filePath, const char *comments);

  /* ---- 1.4.0 ---- */
  uint32_t (*DiscardFrameCapture) (RENDERDOC_DevicePointer device,
                                   RENDERDOC_WindowHandle wndHandle);

  /* ---- 1.5.0 ---- */
  uint32_t (*ShowReplayUI) (void);

  /* ---- 1.6.0 ---- */
  void (*SetCaptureTitle) (const char *title);

  /* ---- 1.7.0 ---- */
  uint32_t (*SetObjectAnnotation) (RENDERDOC_DevicePointer device, void *object,
                                   const char *key,
                                   RENDERDOC_AnnotationType valueType,
                                   uint32_t valueVectorWidth,
                                   const RENDERDOC_AnnotationValue *value);

  uint32_t (*SetCommandAnnotation) (RENDERDOC_DevicePointer device,
                                    void *queueOrCommandBuffer, const char *key,
                                    RENDERDOC_AnnotationType valueType,
                                    uint32_t valueVectorWidth,
                                    const RENDERDOC_AnnotationValue *value);
} RENDERDOC_API_1_7_0;

#if defined(__cplusplus)
} /* extern "C" */

/* ------------------------------------------------------------------ */
/* C++ helpers                                                         */
/* ------------------------------------------------------------------ */

struct RDGLObjectHelper
{
  void *object = nullptr;
  RENDERDOC_ResourceId id{};
};

struct RDAnnotationHelper
{
  RENDERDOC_AnnotationType type = eRENDERDOC_Empty;
  uint32_t width = 1;
  RENDERDOC_AnnotationValue value{};

  RDAnnotationHelper () = default;

  explicit RDAnnotationHelper (bool b)
  {
    type = eRENDERDOC_Bool;
    value.boolean = b ? 1 : 0;
  }
  explicit RDAnnotationHelper (int32_t i)
  {
    type = eRENDERDOC_Int32;
    value.i32 = i;
  }
  explicit RDAnnotationHelper (uint32_t u)
  {
    type = eRENDERDOC_UInt32;
    value.u32 = u;
  }
  explicit RDAnnotationHelper (int64_t i)
  {
    type = eRENDERDOC_Int64;
    value.i64 = i;
  }
  explicit RDAnnotationHelper (uint64_t u)
  {
    type = eRENDERDOC_UInt64;
    value.u64 = u;
  }
  explicit RDAnnotationHelper (float f)
  {
    type = eRENDERDOC_Float;
    value.f32 = f;
  }
  explicit RDAnnotationHelper (double d)
  {
    type = eRENDERDOC_Double;
    value.f64 = d;
  }
  explicit RDAnnotationHelper (const char *s)
  {
    type = eRENDERDOC_String;
    value.str = s;
  }
  explicit RDAnnotationHelper (RENDERDOC_ResourceId obj)
  {
    type = eRENDERDOC_APIObject;
    value.apiObject = obj;
  }
};

#endif /* __cplusplus */

#endif /* RENDERDOC_APP_H */
