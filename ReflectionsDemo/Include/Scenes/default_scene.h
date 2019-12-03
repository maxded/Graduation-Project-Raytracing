#pragma once

#include "scene.h"

class DefaultScene : public Scene
{
public:
	DefaultScene();
	virtual ~DefaultScene();

	void Load(const std::string& filename, CommandList& command_list);

	void Update(UpdateEventArgs& e) override;

	void Render(CommandList& command_list) override;
protected:

private:

};