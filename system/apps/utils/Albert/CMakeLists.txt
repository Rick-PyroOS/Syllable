# Albert: the calculator (applications/Calculator/Calculator)
syllable_program(calculator OUTPUT_NAME Calculator
  SOURCES calc.cpp calculator.cpp stack.cpp calcbuttons.cpp calcdisplay.cpp togglebutton.cpp
          remsa.cpp base-n.cpp postoffice.cpp aboutwin.cpp
  LIBS libsyllable m INSTALL applications/Calculator)
syllable_resources(calculator FILES Calculator.png icon48x48.png)
syllable_catalogs(calculator NAME Calculator DIR resources LANGS bg da de el es fr hu nl pt-br ru sv EMBED)
