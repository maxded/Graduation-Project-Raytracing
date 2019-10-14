#pragma once

#include <Scene.h>

class SponzaScene : public Scene
{
public:
	SponzaScene();
	virtual ~SponzaScene();

	void Load(const std::string& filename, CommandList& commandList);

	void Update(UpdateEventArgs& e);

	void Render(CommandList& commandList) override;
protected:

private:

};