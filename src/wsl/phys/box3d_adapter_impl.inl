#if WSL_HAS_BOX3D

namespace wsl::phys::box3d
{

struct world::impl
{
  b3WorldId id = b3_nullWorldId;
  std::vector<body_handle> bodies;

  ~impl ()
  {
    if (b3World_IsValid (id)) {
      b3DestroyWorld (id);
    }
  }
};

namespace
{

b3Vec3
to_box3d (vector3 value)
{
  return { value.x, value.y, value.z };
}

b3Quat
to_box3d (quaternion value)
{
  return { { value.x, value.y, value.z }, value.w };
}

vector3
from_box3d (b3Vec3 value)
{
  return { value.x, value.y, value.z };
}

quaternion
from_box3d (b3Quat value)
{
  return { value.v.x, value.v.y, value.v.z, value.s };
}

b3BodyId
decode_body (body_handle body)
{
  return b3LoadBodyId (body);
}

} // namespace

world::world (float gravity_y, std::uint32_t worker_count)
  : m_impl (std::make_unique<impl> ())
{
  b3WorldDef definition = b3DefaultWorldDef ();
  definition.gravity = { 0.0F, gravity_y, 0.0F };
  definition.workerCount = worker_count == 0 ? 1 : worker_count;
  m_impl->id = b3CreateWorld (&definition);
}

world::~world () = default;

world::world (world &&other) noexcept = default;

world &
world::operator= (world &&other) noexcept = default;

void
world::clear ()
{
  if (valid ()) {
    for (body_handle body : m_impl->bodies) {
      if (b3Body_IsValid (decode_body (body))) {
        b3DestroyBody (decode_body (body));
      }
    }
    m_impl->bodies.clear ();
  }
}

std::vector<sensor_event>
world::drain_sensor_events ()
{
  std::vector<sensor_event> result;
  if (!valid ()) {
    return result;
  }

  const b3SensorEvents events = b3World_GetSensorEvents (m_impl->id);
  auto convert = [&] (b3ShapeId sensor_shape, b3ShapeId visitor_shape,
                      bool entered) {
    if (!b3Shape_IsValid (sensor_shape) || !b3Shape_IsValid (visitor_shape)) {
      return;
    }
    result.push_back (
        { b3StoreBodyId (b3Shape_GetBody (sensor_shape)),
          b3StoreBodyId (b3Shape_GetBody (visitor_shape)), entered });
  };
  for (int i = 0; i < events.beginCount; ++i) {
    convert (events.beginEvents[i].sensorShapeId,
             events.beginEvents[i].visitorShapeId, true);
  }
  for (int i = 0; i < events.endCount; ++i) {
    convert (events.endEvents[i].sensorShapeId,
             events.endEvents[i].visitorShapeId, false);
  }
  return result;
}

bool
world::valid () const noexcept
{
  return m_impl != nullptr && b3World_IsValid (m_impl->id);
}

void
world::step (float time_step, int sub_step_count)
{
  if (valid () && time_step > 0.0F && sub_step_count > 0) {
    b3World_Step (m_impl->id, time_step, sub_step_count);
  }
}

void
world::set_gravity (vector3 value)
{
  if (valid ()) {
    b3World_SetGravity (m_impl->id, to_box3d (value));
  }
}

vector3
world::gravity () const
{
  return valid () ? from_box3d (b3World_GetGravity (m_impl->id)) : vector3{};
}

body_handle
world::create_body (const body_desc &desc)
{
  if (!valid ()) {
    return null_body;
  }

  b3BodyDef body_definition = b3DefaultBodyDef ();
  body_definition.type = static_cast<b3BodyType> (desc.type);
  body_definition.position = to_box3d (desc.position);
  body_definition.rotation = to_box3d (desc.rotation);

  const b3BodyId body = b3CreateBody (m_impl->id, &body_definition);
  if (!b3Body_IsValid (body)) {
    return null_body;
  }

  b3ShapeDef shape_definition = b3DefaultShapeDef ();
  shape_definition.density = desc.density;
  shape_definition.isSensor = desc.sensor;
  shape_definition.enableSensorEvents = desc.sensor;

  b3ShapeId shape = b3_nullShapeId;
  if (desc.shape == shape_type::box) {
    const b3BoxHull hull = b3MakeBoxHull (
        desc.half_extents.x, desc.half_extents.y, desc.half_extents.z);
    shape = b3CreateHullShape (body, &shape_definition, &hull.base);
  } else {
    const b3Sphere sphere{ { 0.0F, 0.0F, 0.0F }, desc.radius };
    shape = b3CreateSphereShape (body, &shape_definition, &sphere);
  }

  if (!b3Shape_IsValid (shape)) {
    b3DestroyBody (body);
    return null_body;
  }

  b3Body_ApplyMassFromShapes (body);
  const body_handle handle = b3StoreBodyId (body);
  m_impl->bodies.push_back (handle);
  return handle;
}

void
world::destroy_body (body_handle body)
{
  if (is_body_valid (body)) {
    b3DestroyBody (decode_body (body));
    auto it = std::find (m_impl->bodies.begin (), m_impl->bodies.end (), body);
    if (it != m_impl->bodies.end ()) {
      m_impl->bodies.erase (it);
    }
  }
}

bool
world::is_body_valid (body_handle body) const
{
  return valid () && body != null_body
         && b3Body_IsValid (decode_body (body));
}

vector3
world::body_position (body_handle body) const
{
  return is_body_valid (body)
             ? from_box3d (b3Body_GetPosition (decode_body (body)))
             : vector3{};
}

quaternion
world::body_rotation (body_handle body) const
{
  return is_body_valid (body)
             ? from_box3d (b3Body_GetRotation (decode_body (body)))
             : quaternion{};
}

void
world::set_body_transform (body_handle body, vector3 position,
                           quaternion rotation)
{
  if (is_body_valid (body)) {
    b3Body_SetTransform (decode_body (body), to_box3d (position),
                         to_box3d (rotation));
  }
}

void
world::add_force (body_handle body, vector3 force)
{
  if (is_body_valid (body)) {
    b3Body_ApplyForceToCenter (decode_body (body), to_box3d (force), true);
  }
}

void
world::add_impulse (body_handle body, vector3 impulse)
{
  if (is_body_valid (body)) {
    b3Body_ApplyLinearImpulseToCenter (decode_body (body), to_box3d (impulse),
                                       true);
  }
}

} // namespace wsl::phys::box3d

#else

namespace wsl::phys::box3d
{

struct world::impl
{
};

world::world (float, std::uint32_t) : m_impl (std::make_unique<impl> ())
{
}

world::~world () = default;
world::world (world &&other) noexcept = default;
world &
world::operator= (world &&other) noexcept = default;
void
world::clear ()
{
}
std::vector<sensor_event>
world::drain_sensor_events ()
{
  return {};
}
bool
world::valid () const noexcept
{
  return false;
}
void
world::step (float, int)
{
}
void
world::set_gravity (vector3)
{
}
vector3
world::gravity () const
{
  return {};
}
body_handle
world::create_body (const body_desc &)
{
  return null_body;
}
void
world::destroy_body (body_handle)
{
}
bool
world::is_body_valid (body_handle) const
{
  return false;
}
vector3
world::body_position (body_handle) const
{
  return {};
}
quaternion
world::body_rotation (body_handle) const
{
  return {};
}
void
world::set_body_transform (body_handle, vector3, quaternion)
{
}
void
world::add_force (body_handle, vector3)
{
}
void
world::add_impulse (body_handle, vector3)
{
}

} // namespace wsl::phys::box3d

#endif
