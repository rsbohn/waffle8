/ simple halt/reset vector pair
/ uses ZP 0000 0001 0002
/ uses 7777
		* 0000
		JMP I 0001
		0200
		7777
		* 7777
		HLT
