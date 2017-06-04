#include <OgrePlane.h>
#include <OgreMeshManager.h>
#include "DemoScene.hpp"
#include "Game.hpp"

DemoScene::DemoScene(Game &game)
  : cameraNode(nullptr)
  , light(nullptr)
{
  std::clog << "Loading demo scene" << std::endl;
  Renderer &renderer = game.getRenderer();

  // Camera
  cameraNode = renderer.getSceneManager().getRootSceneNode()->createChildSceneNode();
  cameraNode->attachObject(&renderer.getCamera());
  cameraNode->setPosition(Ogre::Vector3(0, 300, 500));
  cameraNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);
  renderer.getCamera().setNearClipDistance(5);

  // Entities
  illidan = AnimatedEntity(renderer, "illidan.mesh");
  illidan.getEntity().setPosition(0, 50, 0);
  illidan.getEntity().getOgre()->setCastShadows(true);

  Ogre::Plane plane(Ogre::Vector3::UNIT_Y, 0);
  Ogre::MeshManager::getSingleton()
    .createPlane("ground",
		 Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
		 plane,
		 1500, 1500, 20, 20,
		 true,
		 1, 5, 5,
		 Ogre::Vector3::UNIT_Z);

  ground = Entity(renderer, "ground");
  ground.getOgre()->setCastShadows(false);
  ground.getOgre()->setMaterialName("rockwall");

  // Lights
  renderer.getSceneManager().setAmbientLight(Ogre::ColourValue(0.0, 0.0, 0.0));
  light = renderer.getSceneManager().createLight("MainLight");
  light->setType(Ogre::Light::LT_DIRECTIONAL);
  light->setDiffuseColour(1.0, 1.0, 1.0);
  light->setSpecularColour(1.0, 1.0, 1.0);
  light->setDirection(-1, -1, -1);
  light->setPosition(50, 100, 50);
  light->setAttenuation(500, 1.0f, 0.007f, 0.0f);

  // Keys callback
  Keyboard::getKeyboard().registerCallback(OIS::KC_I, [this](bool b) {
    if (!b) {
      illidan.addAnimation("Attack", true, false);
    }
    return (true);
  });
  std::clog << "End loading" << std::endl;
}

bool DemoScene::update(Game &, Ogre::FrameEvent const &fe)
{
  illidan.updateAnimations(fe.timeSinceLastFrame);

  // Will add an iterator shortly
  if (Keyboard::getKeys()[OIS::KC_A]) {
    cameraNode->roll(Ogre::Degree(2));
  }
  if (Keyboard::getKeys()[OIS::KC_E]) {
    cameraNode->roll(Ogre::Degree(-2));
  }
  if (Keyboard::getKeys()[OIS::KC_D]) {
    cameraNode->translate(10, 0, 0);
  }
  if (Keyboard::getKeys()[OIS::KC_Z]) {
    cameraNode->translate(0, 0, -10);
  }
  if (Keyboard::getKeys()[OIS::KC_Q]) {
    cameraNode->translate(-10, 0, 0);
  }
  if (Keyboard::getKeys()[OIS::KC_S]) {
    cameraNode->translate(0, 0, 10);
  }

  if (Keyboard::getKeys()[OIS::KC_U]) {
    illidan.stopAnimation();
  }
  if (Keyboard::getKeys()[OIS::KC_O]) {
    illidan.setAnimation("Move", false);
  }
  if (Keyboard::getKeys()[OIS::KC_P]) {
    illidan.setAnimation("Stand", false);
  }
  return (true);
}
