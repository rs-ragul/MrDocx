include ../makefiles/flags.inc
OBJDIR = ../obj/app
TARGET = ../bin/MrDocx.exe

APP_SRCS = ../src/app.cpp ../src/icons.cpp ../src/main.cpp ../src/pdfengine.cpp \
           ../src/ui_helpers.cpp ../src/ui_layout.cpp ../src/ui_widgets.cpp \
           ../src/views.cpp ../src/views_tool.cpp
APP_OBJS = $(patsubst ../src/%.cpp,$(OBJDIR)/%.o,$(APP_SRCS))
RC_OBJ   = $(OBJDIR)/app_res.o

LIBS = -L. -lpdfengine -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -lole32 -loleaut32 \
       -lshell32 -lshlwapi -lgdiplus -ladvapi32 -ldwmapi -lmsimg32 -luuid -lkernel32

$(TARGET): $(APP_OBJS) $(RC_OBJ) libpdfengine.a
	@mkdir -p ../bin
	$(CXX) $(APP_CXXFLAGS) -municode -static -o $@ $(APP_OBJS) $(RC_OBJ) $(LIBS) -Wl,--gc-sections
	@echo APP_DONE

$(OBJDIR)/%.o: ../src/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(APP_CXXFLAGS) -c $< -o $@

$(RC_OBJ): ../res/app.rc ../res/app.ico ../res/app.manifest ../res/logo.png
	@mkdir -p $(OBJDIR)
	$(WRC) -I ../res -O coff -o $@ $<

.PHONY: $(TARGET)
