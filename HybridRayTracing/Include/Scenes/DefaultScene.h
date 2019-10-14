#pragma once

#include <Scene.h>

class DefaultScene : public Scene
{
public:
	DefaultScene();
	virtual ~DefaultScene();

	void Load(const std::string& filename, CommandList& commandList);

	void Update(UpdateEventArgs& e);

	void Render(CommandList& commandList);
protected:

private:

};