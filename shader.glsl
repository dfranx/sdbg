#version 330

out vec4 outColor;
uniform vec4 uVal;

void main()
{
	float val = uVal.x;
	
	for (int i = 0; i < 5; i++) 
		val *= 0.5f;
		
	outColor = vec4(uVal.xyz, val);
}