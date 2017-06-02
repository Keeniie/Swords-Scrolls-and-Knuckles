#ifndef UIMANAGER_HPP
# define UIMANAGER_HPP

# include <Overlay/OgreOverlayManager.h>
# include "UIOverlay.hpp"
# include "UIOverlayHUD.hpp"
# include "UIOverlayMenu.hpp"

class UIManager {

	public:
		UIManager();
		UIManager(UIManager const &) = delete;
		UIManager(UIManager &&) = delete;
		UIManager &operator=(UIManager const &) = delete;
		UIManager &operator=(UIManager &&) = delete;
		~UIManager(void) = default;
		
		void showOverlayByName(std::string const &) const;
		void hideOverlayByName(std::string const &) const;
		void hideAllOverlays(void) const;
		UIOverlay *getByName(Ogre::String const &) const;
		
	private:
		Ogre::OverlayManager *overlayManager;
		std::map<Ogre::String, UIOverlay *> overlays;
		
	protected:
};

#endif // !UIMANAGER_HPP
