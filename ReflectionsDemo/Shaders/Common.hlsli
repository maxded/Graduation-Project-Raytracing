static const float M_PI = 3.141592653589793f;

static const float M_RMIN = 0.01; // 1 cm.
static const float M_RMAX = 5.0;  // 10 m.
static const float M_INF  = 9999.9;

// Inverse-square equation.
float InverseSquare(float r)
{
	return 1.0 / (pow(max(r, M_RMIN), 2.0));
}

// Windowing functions used by Unreal Engine & Frostbite Engine.
float Windowing(float r)
{
	float result = 1.0 - (pow(r / M_RMAX, 4));

	return pow(clamp(result, 0.0, M_INF), 2.0);
}

