#include <OgreParticleSystem.h>
#include <algorithm>
#include <iostream>
#include "UIOverlaySelection.hpp"
#include "Logic.hpp"
#include "Physics.hpp"
#include "LevelScene.hpp"
#include "Player.hpp"
#include "Enemy.hpp"
#include "AudioListener.hpp"

// TODO: extract as mush as possible to gameState.
// Logic could be passed as ref for spawning and & so on.
bool Logic::tick()
{
  std::lock_guard<std::mutex> const lock_guard(lock);

  ++updatesSinceLastFrame;

  auto const updateElements([this](auto &elements)
			    {
			      for (auto &element : elements)
				{
				  element.update(*this);
				  gameState.terrain.correctFixture
				    (element,
				     [](auto &element, Vect<2u, double> dir)
				     {
				       if (element.isStun())
					 BounceResponse{0.5}(element, dir);
				     });
				}
			    });
  updateElements(gameState.enemies);
  for (auto &enemy : gameState.enemies)
    {
      if (enemy.shouldBeRemoved())
	{
	  int dropSeed(std::uniform_int_distribution<>(0, 15)(randEngine));

	  auto drop((dropSeed == 0) ? ProjectileType::COOLDOWN_RESET :
		    (dropSeed == 1) ? ProjectileType::GOLD50 :
		    (dropSeed <= 3) ? ProjectileType::GOLD20 :
		    (dropSeed <= 6) ? ProjectileType::GOLD5 :
		    (dropSeed <= 10) ? ProjectileType::HEAL :  ProjectileType::GOLD);

	  enemyProjectiles.add([this, drop](){
	      return entityFactory.spawnProjectile(drop);
	    }, enemy.pos, Vect<2u, double>{0.0, 0.0}, drop, 0.5);
	}
    }
  updateElements(gameState.players);
  for (auto &player : gameState.players)
    {
      auto &room(gameState.terrain.getRoom(Vect<2u, unsigned int>(player.pos)));

      player.checkSpells(*this);
      if (!room.mobsSpawned)
	{
	  spawnMobGroup(room);
	  std::cout << "spawning mobs" << std::endl;
	}
    }
  auto const updateProjectile([this](auto &projectiles) {
      for (auto &projectile : projectiles)
	{
	  projectile.update(*this);
	  gameState.terrain.correctFixture(projectile,
					   [this](auto &projectile, Vect<2u, double> dir) {
					     projectileList[projectile.type].wallResponse(projectile, dir);
					   });
	  if (projectile.type == ProjectileType::EXPLOSION)
	    particleSpawns.emplace_back(projectile.pos, "explosion");
	  else if (projectile.type == ProjectileType::HIT1
		   || ((projectile.type == ProjectileType::ARROW
			|| projectile.type == ProjectileType::BOUNCY_ARROW
			|| projectile.type == ProjectileType::ICE_PILLAR)
		       ))
	    particleSpawns.emplace_back(projectile.pos, "blu");
	}
    });
  updateProjectile(gameState.projectiles);
  updateProjectile(gameState.enemyProjectiles);
  projectiles.removeIf([](auto const &projectile)
		       {
			 return projectile.shouldBeRemoved();
		       });
  enemyProjectiles.removeIf([](auto const &projectile)
			    {
			      return projectile.shouldBeRemoved();
			    });
  enemies.removeIf([](auto const &projectile)
		   {
		     return projectile.shouldBeRemoved();
		   });
  Physics::collisionTest(gameState.players.begin(), gameState.players.end(),
			 gameState.enemies.begin(), gameState.enemies.end(),
			 [](auto &player, auto &enemy){
			   player.knockback((player.pos - enemy.pos).normalized() * 0.15, 5);
			   player.takeDamage(30);
			 });
  Physics::collisionTest(gameState.projectiles.begin(), gameState.projectiles.end(),
			 gameState.enemies.begin(), gameState.enemies.end(),
			 [this](auto &projectile, auto &enemy){
			   projectileList[projectile.type].hitEnemy(enemy, projectile);
			 });
  Physics::collisionTest(gameState.enemyProjectiles.begin(), gameState.enemyProjectiles.end(),
			 gameState.players.begin(), gameState.players.end(),
			 [this](auto &enemyProjectile, auto &player){
			   if (enemyProjectile.type == ProjectileType::COOLDOWN_RESET)
			     player.resetCooldowns();
			   else if (enemyProjectile.type >= ProjectileType::GOLD
				    && enemyProjectile.type <= ProjectileType::GOLD50)
			     player.addGold(Vect<4u, unsigned int>(1u, 5u, 20u, 50u)[enemyProjectile.type - ProjectileType::GOLD]);
			   projectileList[enemyProjectile.type].hitEnemy(player, enemyProjectile);
			 });
  constexpr auto const correctOverlap([](auto &a, auto &b){
      auto const center((a.pos + b.pos) * 0.5);
      auto const overlap((a.pos - b.pos).normalized() * (a.radius + b.radius));

      a.pos = center + overlap * 0.5;
      b.pos = center - overlap * 0.5;
    });
  Physics::collisionTest(gameState.players.begin(), gameState.players.end(), correctOverlap);
  Physics::collisionTest(gameState.enemies.begin(), gameState.enemies.end(), correctOverlap);
  for (auto &enemy : gameState.enemies)
  {
    if (enemy.ai)
    {
      pyBindInstance.execAI[enemy.ai](&pyBindInstance, enemy, pyEvaluate);
    }
  }
  for (auto &player : gameState.players)
  {
    unsigned int ai(player.getAI());

    if (ai)
    {
      pyBindInstance.execAI[ai](&pyBindInstance, player, pyEvaluate);
      player.setAttacking(0u, pyEvaluate.attack);
      player.setAttacking(2u, pyEvaluate.attack);
    }
  }
  return stop;
}

Logic::Logic(LevelScene &levelScene, Renderer &renderer, std::vector<AnimatedEntity> &playerEntities, std::vector<PlayerId> const &vec, std::vector<Gameplays> const &gp)
  : stop(false)
  , playerEntities(playerEntities)
  , enemies(gameState.enemies, levelScene.enemies)
  , projectiles(gameState.projectiles, levelScene.projectiles)
  , enemyProjectiles(gameState.enemyProjectiles, levelScene.enemyProjectiles)
  , entityFactory(renderer)
  , pyEvaluate(gameState.players, gameState.enemies, gameState.terrain)
  , projectileList{}
  , spellList{}
  , randEngine(42u)
  , keyboardControllers{
      std::map<unsigned int, OIS::KeyCode>
#if defined OIS_WIN32_PLATFORM
      { {KBACTION::GO_UP, OIS::KC_W}, {KBACTION::GO_DOWN, OIS::KC_S},
      {KBACTION::GO_LEFT, OIS::KC_A}, {KBACTION::GO_RIGHT, OIS::KC_D},
      {KBACTION::SPELL1, OIS::KC_V}, {KBACTION::SPELL2, OIS::KC_B},
      {KBACTION::SPELL3, OIS::KC_N}, {KBACTION::LOCK, OIS::KC_LSHIFT},
      {KBACTION::MOUNT, OIS::KC_Z}},
#else
      {{KBACTION::GO_UP, OIS::KC_Z}, {KBACTION::GO_DOWN, OIS::KC_S},
      {KBACTION::GO_LEFT, OIS::KC_Q}, {KBACTION::GO_RIGHT, OIS::KC_D},
      {KBACTION::SPELL1, OIS::KC_V}, {KBACTION::SPELL2, OIS::KC_B},
      {KBACTION::SPELL3, OIS::KC_N}, {KBACTION::LOCK, OIS::KC_LSHIFT},
      {KBACTION::MOUNT, OIS::KC_W}},
#endif // defined OIS_WIN32_PLATFORM
      std::map<unsigned int, OIS::KeyCode>
#if defined OIS_WIN32_PLATFORM
      { {KBACTION::GO_UP, OIS::KC_O}, {KBACTION::GO_DOWN, OIS::KC_L},
      {KBACTION::GO_LEFT, OIS::KC_K}, {KBACTION::GO_RIGHT, OIS::KC_SEMICOLON},
      {KBACTION::SPELL1, OIS::KC_LEFT}, {KBACTION::SPELL2, OIS::KC_RIGHT},
      {KBACTION::SPELL3, OIS::KC_UP}, {KBACTION::LOCK, OIS::KC_RSHIFT},
      {KBACTION::MOUNT, OIS::KC_DOWN}} }
#else
      { {KBACTION::GO_UP, OIS::KC_O}, {KBACTION::GO_DOWN, OIS::KC_L},
      {KBACTION::GO_LEFT, OIS::KC_K}, {KBACTION::GO_RIGHT, OIS::KC_M},
      {KBACTION::SPELL1, OIS::KC_LEFT}, {KBACTION::SPELL2, OIS::KC_RIGHT},
      {KBACTION::SPELL3, OIS::KC_UP}, {KBACTION::LOCK, OIS::KC_RSHIFT},
      {KBACTION::MOUNT, OIS::KC_DOWN}}}
#endif // defined OIS_WIN32_PLATFORM
{
  gameState.terrain.generateLevel(420u); // TODO: something better
  for (size_t i = 0; i < vec.size(); i++) {
    gameState.players.push_back(Player::makePlayer(Vect<2u, double>{(double)i + 8.0, (double)(i % 2) + 8.0}, vec[i]));
  }
  size_t kb = 0;
  size_t js = 0;
  for (size_t i = 0; i < gp.size(); i++) {
    if (gp[i] == Gameplays::KEYBOARD) {
      action.keyboardControlled[&keyboardControllers[kb]] = &gameState.players[i];
      kb++;
    }
    else if (gp[i] == Gameplays::JOYSTICK && Joystick::getJoysticks()[js]) {
      action.joystickControlled[Joystick::getJoysticks()[js].get()] = &gameState.players[i];
      js++;
    }
    else if (gp[i] == Gameplays::IA) {
      PlayerId id(static_cast<PlayerId>(gameState.players[i].getId()));

      for (auto &player : gameState.players)
      {
        unsigned int ai(player.getAI());

        if (ai == 0 || ai == AI::LEADERCONTACTAI || ai == AI::LEADERDISTANCEAI)
        {
          if (id == PlayerId::ARCHER || id == PlayerId::MAGE)
            gameState.players[i].setAI(AI::COMPANIONDISTANCEAI);
          else if (id == PlayerId::TANK || id == PlayerId::WARRIOR)
            gameState.players[i].setAI(AI::COMPANIONCONTACTAI);
        }
      }
      if (!gameState.players[i].getAI())
      {
        if (id == PlayerId::ARCHER || id == PlayerId::MAGE)
          gameState.players[i].setAI(AI::LEADERDISTANCEAI);
        else if (id == PlayerId::TANK || id == PlayerId::WARRIOR)
          gameState.players[i].setAI(AI::LEADERCONTACTAI);
      }
    }
  }
  levelScene.setTerrain(gameState.terrain);
}

void Logic::spawnMobGroup(Terrain::Room &room)
{
  room.mobsSpawned = true;
  std::clog << "[Logic] Spawning " << room.id / 2u + 5u << " mobs at : " << room.pos << std::endl;
  for (unsigned int i(0u); i < room.id / 2u + 5u; ++i)
    enemies.add([this](){
	return entityFactory.spawnEnemy();
      }, AI::CHASEPLAYER, 100u * gameState.players.size(), 0.5, room.pos + Vect<2u, double>{0., (double)i * 0.1});
}

void Logic::spawnProjectile(Vect<2u, double> pos, Vect<2u, double> speed, unsigned int type, double size, unsigned int timeLeft)
{
  projectiles.add([this](){
      return entityFactory.spawnOgreHead();
    }, pos, speed, type, size, timeLeft);
}

void Logic::run()
{
  constexpr std::chrono::microseconds TICK_TIME{1000000 / 120};

  lastUpdate = Clock::now();
  updatesSinceLastFrame = 0;
  while (!tick())
    {
      auto const now(Clock::now());

      if (now > lastUpdate + TICK_TIME * 3)
	{
	  lastUpdate = now;
	  continue ;
	}
      lastUpdate += TICK_TIME;
      if (now < lastUpdate)
	std::this_thread::sleep_for(lastUpdate - now);
    }
  std::clog << "[Logic] thread exiting" << std::endl;
}

void Logic::exit()
{
  std::lock_guard<std::mutex> const lock_guard(lock);

  stop = true;
  std::clog << "[Logic] stoping thread" << std::endl;
}

void Logic::updateDisplay(LevelScene &levelScene)
{
  std::lock_guard<std::mutex> const lock_guard(lock);

  enemies.updateTarget();
  auto const updateProjectileEntities([this, &levelScene](auto &projectiles){
      projectiles.updateTarget();
      projectiles.forEach([this](Entity &entity, Projectile &projectile)
			  {
			    double angle(projectile.timeLeft * 0.01);

			    if (projectile.doSpin())
			      entity.setDirection(Vect<2u, Ogre::Real>((Ogre::Real)std::cos(angle), (Ogre::Real)std::sin(angle)));
			    entity.setPosition(static_cast<Ogre::Real>(projectile.pos[0]), 0.f, static_cast<Ogre::Real>(projectile.pos[1]));
			  });
    });
  updateProjectileEntities(projectiles);
  updateProjectileEntities(enemyProjectiles);

  for (auto &&pair : particleSpawns)
    {
      particleEffects.emplace_back(30, entityFactory.createParticleSystem(pair.second));
      particleEffects.back().second.setPosition(static_cast<Ogre::Real>(pair.first[0]), 0.f, static_cast<Ogre::Real>(pair.first[1]));	
    }
  particleSpawns.clear();
  for (auto &effect : particleEffects)
    {
      if (effect.first > updatesSinceLastFrame)
	effect.first -= updatesSinceLastFrame;
      else
	effect.first = 0;
      if (effect.first < 10)
	effect.second.getOgre()->setEmitting(false);
      // if (!effect.first)
      // 	effect.second = ParticleEffect{}; // Pre-erase just in case.
    }
  particleEffects.erase(std::remove_if(particleEffects.begin(), particleEffects.end(), [](auto const &p)
				       {
					 return !p.first;
				       }), particleEffects.end());


  auto const updateControllableEntity([](AnimatedEntity &animatedEntity, Controllable &controllable){
      animatedEntity.getEntity().setDirection(controllable.getDir());
      animatedEntity.getEntity().setPosition(static_cast<Ogre::Real>(controllable.pos[0]),
					     animatedEntity.isMounted(), // Put the controllable a bit higher when he's on his mount.
					     static_cast<Ogre::Real>(controllable.pos[1])
					     );
    });
  enemies.forEach([updateControllableEntity, this](AnimatedEntity &animatedEntity, Enemy &enemy)
		  {
		    updateControllableEntity(animatedEntity, enemy);
		    if (enemy.isDead())
		      animatedEntity.setMainAnimation(Animations::Controllable::Enemy::DEATH, 0.04f, false);
		    else if (enemy.isWalking())
		      animatedEntity.setMainAnimation(Animations::Controllable::WALK);
		    else if (enemy.isStun())
		      animatedEntity.setMainAnimation(Animations::Controllable::STUN);
		    else
		      animatedEntity.setMainAnimation(Animations::Controllable::STAND);
		    animatedEntity.updateAnimations(static_cast<Ogre::Real>(updatesSinceLastFrame * (1.0f / 120.0f)));
		  });

  for (unsigned int i(0); i != gameState.players.size(); ++i)
    {
      AnimatedEntity &animatedEntity(playerEntities[i]);
      Player &player(gameState.players[i]);
      bool otherMainAnimation{false};

      updateControllableEntity(animatedEntity, player);

      switch (static_cast<PlayerId>(player.getId()))
	{
	case PlayerId::ARCHER:
	  if (player.getSpells()[0].startedSince() <= updatesSinceLastFrame)
	    animatedEntity.addSubAnimation(Animations::Controllable::Player::ATTACK, true);
	  if (player.getSpells()[1].hasEffect())
	    {
	      animatedEntity.setMainAnimation(Animations::Controllable::Archer::JUMP, 0.1f, false);
	      otherMainAnimation = true;
	    }
	  if (player.getSpells()[2].startedSince() <= updatesSinceLastFrame)
	    animatedEntity.addSubAnimation(Animations::Controllable::Mage::SPELL_C, true);
	  if (player.getSpells()[2].hasEffect())
	    {
	      Ogre::Real scale;

	      if (player.getSpells()[2].startedSince() < 60)
		{
		  scale = (1.0f + player.getSpells()[2].startedSince() / 60.0f) / 150.0f;
		  otherMainAnimation = true;
		}
	      else if (player.getSpells()[2].startedSince() > 420)
		scale = (1.0f + (480.0f - player.getSpells()[2].startedSince()) / 60.0f) / 150.0f;
	      else
		scale = 2.0f / 150.0f;
	      animatedEntity.getEntity().getNode()->setScale(scale, scale, scale);
	    }
	  break;
	case PlayerId::MAGE:
	  if (player.getSpells()[0].startedSince() <= updatesSinceLastFrame)
	    animatedEntity.addSubAnimation(Animations::Controllable::Mage::SPELL_E, true);
	  if (player.getSpells()[1].hasEffect())
	    animatedEntity.addSubAnimation(Animations::Controllable::Mage::SPELL_E, true);
	  if (player.getSpells()[2].hasEffect() != animatedEntity.isMounted()) {
	    animatedEntity.setMounted(player.isMounted());
	  }
	  break;
	case PlayerId::TANK:
	  if (player.getSpells()[0].startedSince() <= updatesSinceLastFrame)
	    animatedEntity.addSubAnimation(Animations::Controllable::Player::ATTACK, true, false);
	  if (player.getSpells()[1].hasEffect())
	    {
	      animatedEntity.setMainAnimation(Animations::Controllable::Tank::JUMP, 0.1f, false);
	      otherMainAnimation = true;
	    }
	  if (player.getSpells()[3].hasEffect())
	    {
	      animatedEntity.addSubAnimation(Animations::Controllable::Tank::SPELL_FORWARD, 0.0f, true);
	    }
	  break;
	case PlayerId::WARRIOR:
	  if (player.getSpells()[0].startedSince() <= updatesSinceLastFrame)
	    animatedEntity.addSubAnimation(Animations::Controllable::Player::ATTACK, true, false);
	  if (player.getSpells()[1].hasEffect())
	    {
	      animatedEntity.setMainAnimation(Animations::Controllable::Warrior::JUMP, 0.1f, false);
	      otherMainAnimation = true;
	    }
	  if (player.getSpells()[2].hasEffect())
	    {
	      double angle(-player.getSpells()[2].startedSince() * 0.1);

	      animatedEntity.setMainAnimation(Animations::Controllable::Warrior::SPELL_FORWARD, 0.1f, true);
	      animatedEntity.getEntity().setDirection(Vect<2u, float>((float)std::cos(angle), (float)std::sin(angle)));
	      otherMainAnimation = true;
	      Ogre::Real scale;

	      if (player.getSpells()[2].startedSince() < 60)
		{
		  scale = (1.0f + player.getSpells()[2].startedSince() / 60.0f) / 150.0f;
		  otherMainAnimation = true;
		}
	      else if (player.getSpells()[2].startedSince() > 420)
		scale = (1.0f + (480.0f - player.getSpells()[2].startedSince()) / 60.0f) / 150.0f;
	      else
		scale = 2.0f / 150.0f;
	      animatedEntity.getEntity().getNode()->setScale(scale, scale, scale);
	    }
	  break;
	}

      if (!otherMainAnimation)
	{
	  if (player.isWalking())
	    {
	      if (!animatedEntity.getEntity().soundMap->at(Sounds::BOYAUX1).isPlaying())
		animatedEntity.getEntity().soundMap->at(Sounds::BOYAUX1).play();
	      if (animatedEntity.isMounted())
		{
		  animatedEntity.setMainAnimation(Animations::Controllable::Player::WALK_RIDE);
		  animatedEntity.getMount()->setMainAnimation(Animations::Controllable::WALK);
		}
	      else
		animatedEntity.setMainAnimation(Animations::Controllable::WALK);
	    }
	  else
	    {
	      if (animatedEntity.getEntity().soundMap->at(Sounds::BOYAUX1).isPlaying())
		animatedEntity.getEntity().soundMap->at(Sounds::BOYAUX1).stop();
	      if (animatedEntity.isMounted())
		{
		  animatedEntity.setMainAnimation(Animations::Controllable::Player::STAND_RIDE);
		  animatedEntity.getMount()->setMainAnimation(Animations::Controllable::STAND);
		}
	      else
		animatedEntity.setMainAnimation(Animations::Controllable::STAND);
	    }
	}
      animatedEntity.updateAnimations(static_cast<Ogre::Real>(updatesSinceLastFrame * (1.0f / 120.0f)));
    }

  action.update();
  calculateCamera(levelScene);
  levelScene.updateUI(gameState.players);
  updatesSinceLastFrame = 0;
}

void Logic::calculateCamera(LevelScene &levelScene)
{
  constexpr double const angle(180 - 60 / 2);
  double const tanAngle(tan(angle));
  constexpr double const angleUp(180 - 80 / 2);
  double const tanAngleUp(tan(angleUp));
  constexpr double const yMax(20.f);
  Vect<3u, double> const cameraPos(levelScene.cameraNode->getPosition().x,
				   levelScene.cameraNode->getPosition().y,
				   levelScene.cameraNode->getPosition().z);

  auto const minmax_x(std::minmax_element(gameState.players.cbegin(),
					  gameState.players.cend(),
					  [](auto const &p1, auto const &p2) {
					    return p1.getPos()[0] < p2.getPos()[0];
					  }));
  Vect<3u, double> const leftVecX(minmax_x.first->getPos()[0], 0.0, minmax_x.first->getPos()[1]);
  Vect<3u, double> const rightVecX(minmax_x.second->getPos()[0], 0.0, minmax_x.first->getPos()[1]);
  auto const midVecX((rightVecX - leftVecX) / 2 - cameraPos);

  auto const minmax_z(std::minmax_element(gameState.players.cbegin(),
					  gameState.players.cend(),
					  [](auto const &p1, auto const &p2) {
					    return p1.getPos()[1] < p2.getPos()[1];
					  }));
  Vect<3u, double> const leftVecZ(minmax_z.first->getPos()[1], 0.0, minmax_z.first->getPos()[1]);
  Vect<3u, double> const rightVecZ(minmax_z.second->getPos()[1], 0.0, minmax_z.first->getPos()[1]);
  auto const midVecZ((rightVecZ - leftVecZ) / 2 - cameraPos);

  double const yxpos((-tanAngle * (std::sqrt(midVecX.length2())) + 10) * 1.5f);
  double const yzpos((-tanAngleUp * (std::sqrt(midVecZ.length2())) + 10) * 0.8f);

  Vect<3u, double> cameraDest;

  cameraDest[0] = minmax_x.first->getPos()[0]
    + (double)(minmax_x.second->getPos()[0] - minmax_x.first->getPos()[0]) / 2.f;
  cameraDest[1] = clamp(std::max(yxpos, yzpos), 0.0, yMax);
  cameraDest[2] = (double)(minmax_z.first->getPos()[1]+ (minmax_z.second->getPos()[1] - minmax_z.first->getPos()[1]) / 2.f)
    + 0.5f * cameraDest[1];

  cameraDest = cameraPos + (cameraDest - cameraPos) / 10.0;
  levelScene.cameraNode->setPosition((Ogre::Real)cameraDest[0],
				     (Ogre::Real)cameraDest[1],
				     (Ogre::Real)cameraDest[2]);

  AudioListener::setPos(levelScene.cameraNode->getPosition());
}

void Logic::pause(void) {
  lock.lock();
}

void Logic::unpause(void) {
  lock.unlock();
}
