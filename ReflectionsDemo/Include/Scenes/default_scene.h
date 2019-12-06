#pragma once

#include "gltf_scene.h"

class DefaultScene : public Scene
{
public:
	DefaultScene();
	virtual ~DefaultScene();

	void Load(const std::string& filename);

	void Update(UpdateEventArgs& e) override;

	void Render(CommandList& command_list) override;
protected:

private:

};