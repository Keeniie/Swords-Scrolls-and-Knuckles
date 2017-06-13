#include <OgreSceneNode.h>
#include "EntityFactory.hpp"
#include "Entity.hpp"
#include "AnimatedEntity.hpp"

Entity EntityFactory::spawnOgreHead(void)
{
  Entity ogre(renderer, "ogrehead.mesh");

  ogre.getOgre()->setCastShadows(true);
  ogre.getNode()->setScale(1.0f / 150.0f, 1.0f / 150.0f, 1.0f / 150.0f);
  return ogre;
}

AnimatedEntity EntityFactory::spawnArcher(Skins::Skin skin)
{
  AnimatedEntity archer(renderer, skin);

  archer.getEntity().getOgre()->setCastShadows(true);
  archer.getEntity().getNode()->setScale(1.0f / 150.0f, 1.0f / 150.0f, 1.0f / 150.0f);

  // Mount
  AnimatedEntity *mount(new AnimatedEntity("wolf.mesh", archer.getEntity().getNode()));
  mount->getEntity().getOgre()->setCastShadows(true);
  mount->getEntity().getNode()->translate(0.f, -150.f, 0.f); // Put the mount under the player
  archer.setMount(mount);
  return (archer);
}
