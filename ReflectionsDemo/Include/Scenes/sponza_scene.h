#pragma once

#include "gltf_scene.h"

class SponzaScene : public Scene
{
public:
	SponzaScene();
	virtual ~SponzaScene();

	void Load(const std::string& filename);

	void Update(UpdateEventArgs& e) override;

	void Render(CommandList& command_list) override;
protected:

private:

};