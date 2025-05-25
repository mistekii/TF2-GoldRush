#base "SourceSchemeBase.res"

Scheme
{
	BaseSettings
	{
		// scheme-specific colors
		MainMenu.TextColor			"TanLight"			[$WIN32]
		MainMenu.MenuItemHeight		"14"
		MainMenu.ArmedTextColor		"117 107 94 255"	[$WIN32]
		MainMenu.Inset				"32"	
	}
	
	Fonts
	{
		"MainMenuFont"
		{
			"1"	[$WIN32]
			{
				"name"		"TF2 Build"
				"tall"		"8"
				"weight"	"500"
				"additive"	"0"
				"antialias" "1"
			}
		}
		"MenuLarge"
		{
			"1"	[$X360]
			{
				"tall_hidef"		"24"
			}
		}
	}
	
	CustomFontFiles
	{
		"9"		"resource/TF2Build.ttf"
	}
}
