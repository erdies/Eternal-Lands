/*!
\brief A special effect that creates a fire (of any size)
*/

#ifdef EYE_CANDY

#ifndef EFFECT_CAMPFIRE_H
#define EFFECT_CAMPFIRE_H

// I N C L U D E S ////////////////////////////////////////////////////////////

#include "eye_candy.h"

namespace ec
{

// C L A S S E S //////////////////////////////////////////////////////////////

class CampfireParticle : public Particle
{
public:
  CampfireParticle(Effect* _effect, ParticleMover* _mover, const Vec3 _pos, const Vec3 _velocity, const float _scale, const float _sqrt_scale, const int _state, const Uint16 _LOD);
  ~CampfireParticle() {}
  
  virtual bool idle(const Uint64 delta_t);
  virtual void draw(const Uint64 usec);
  virtual GLuint get_texture(const Uint16 res_index);
  virtual light_t estimate_light_level() const { return 0.003; };	// We don't want the particle system lights to be used on the pos, since it will assumedly already have one.

  coord_t size_max;  
  Uint16 LOD;
};

class CampfireBigParticle : public Particle
{
public:
  CampfireBigParticle(Effect* _effect, ParticleMover* _mover, const Vec3 _pos, const Vec3 _velocity, const float _sqrt_scale, const Uint16 _LOD);
  ~CampfireBigParticle() {}
  
  virtual bool idle(const Uint64 delta_t);
  virtual GLuint get_texture(const Uint16 res_index);
  virtual light_t estimate_light_level() const { return 0.003; };	// Like above
  virtual bool deletable() { return false; };
};

class CampfireEffect : public Effect
{
public: 
  CampfireEffect(EyeCandy* _base, bool* _dead, Vec3* _pos, std::vector<ec::Obstruction*>* _obstructions, const float _scale, const Uint16 _LOD);
  ~CampfireEffect(); 
  
  virtual EffectEnum get_type() { return EC_CAMPFIRE; };
  bool idle(const Uint64 usec);

  ParticleMover* mover;
  ParticleMover* stationary;
  ParticleSpawner* spawner;
  int big_particles;
  float scale;
  float sqrt_scale;
};

///////////////////////////////////////////////////////////////////////////////

}	// End namespace ec

#endif	// defined EFFECT_CAMPFIRE_H

#endif	// #ifdef EYE_CANDY