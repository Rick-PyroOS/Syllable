//
// Created by Rick Caudill (rick.g.caudill@gmail.com) on 2022-01-16.
//

//This is used to open a file using the registrar through the command line

#include <storage/registrar.h>
#include <storage/path.h>
#include <gui/image.h>
#include <util/application.h>
#include <gui/guidefines.h>
#include <util/string.h>
#include <util/optionparser.h>
#include <unistd.h>
#include <storage/fsnode.h>
#include <sys/stat.h>
#include <vector>

bool fileExists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

void usage(os::OptionParser &cOpts, bool bFull)
{
    std::string cCmd = cOpts.GetArgs()[0];
    uint nPos = cCmd.rfind('/');
    if (nPos != std::string::npos)
    {
        cCmd = std::string(cCmd.begin() + nPos + 1, cCmd.end());
    }

    if (bFull)
    {
        printf("\n");
        printf("Opens a file from the shell\n");
        printf("\n");
        cOpts.PrintHelpText();
    }
    exit(0);
}


class Application : public os::Application
{
public:
    Application(int argc, char **argv) : os::Application("x-vnd.open-file")
    {
        m_pcManager = os::RegistrarManager::Get();

        os::OptionParser cOpts(argc, argv);
        os::OptionParser::argmap asArgs[] = {
                {"list",   'l', "List all known application handlers for this file type"},
                {"open",   'a', "Open with the specified application handler"},
                {"edit",   'e', "Open with default editor"},
                {"reveal", 'r', "Opens in default file browser instead"},
                {"prompt", 'p', "Opens up a prompt to select the application handler"},
                {"help",   'h', "Print this help message"},
                {NULL,     0,   NULL}
        };

        cOpts.AddArgMap(asArgs);
        cOpts.ParseOptions("laerph");

        if (argc <= 1 || cOpts.FindOption('h'))
        {
            usage(cOpts, true);
        }
        else
        {
            if (fileExists(cOpts.GetFileArgs()[0].c_str()))
            {
                const os::OptionParser::option *option = cOpts.GetOption(0);

                if (option != NULL)
                {
                    switch (option->opt)
                    {
                        case 'l':
                        {
                            if  (argc == 3) {
								os::Path path = os::Path(cOpts.GetFileArgs()[0]);
								List(path.GetPath());
							} else{
								printf("You must pass arguments like: open --list/-l filename");
							}
                            exit(0);
                            break;
                        }
                        case 'r':
                        {
							if  (argc == 3) {
								os::Path path = os::Path(cOpts.GetFileArgs()[0]);
								RevealFile(path);
							} else{
								printf("You must pass arguments like: open --reveal/-r filename");
							}
                            exit(0);
                            break;
                        }

                        case 'e':
                        {
							if  (argc == 3) {

								os::Path path = os::Path(cOpts.GetFileArgs()[0]);
								OpenFile(path.GetPath(), "/system/bin/aedit");
							} else{
								printf("You must pass arguments like: open --edit/-e filename");
							}
                            exit(0);
                            break;
                        }

                        case 'a':
                        {
                            os::Path path = os::Path(cOpts.GetFileArgs()[0]);
                            os::String handler;
                            OpenFile(path.GetPath(), handler);
                            exit(0);
                            break;
                        }

                        case 'p':
                        {
                            os::Path path = os::Path(cOpts.GetFileArgs()[0]);
                            os::String handler;
                            Prompt(path.GetPath());
                            break;
                        }

                        default:
                            printf("Invalid option\n");
                            exit(1);
                            break;
                    }
                }

                else
                {
                    if (argc >= 2)
                    {
                        os::FSNode *node = new os::FSNode(argv[1]);

                        if (node->IsDir())
                        {
                            os::Path path = os::Path(argv[1]);
                            RevealFile(path);
                        }

                        else if (node->IsFile())
                        {
                            os::Path path = os::Path(argv[1]);
                            OpenFile(path.GetPath());
                        }
                        exit(0);
                    }
                    else
                    {
                        exit(1);
                    }
                }
            }
        }
    }

    ~Application()
    {
        m_pcManager->Put();
    }

    void HandleMessage(os::Message *pcMessage);

    void OpenFile(const os::String &cFile, const os::String &handler = "");

    void List(const os::String &cFile);

    void RevealFile(const os::Path &cPath);

    void Prompt(const os::String &cFile);

    bool OkToQuit();

private:
    os::RegistrarManager *m_pcManager;
};

bool Application::OkToQuit()
{
    return true;
}

void Application::HandleMessage(os::Message *message)
{
}

void Application::Prompt(const os::String &cFile)
{
    int status = m_pcManager->Launch(NULL, cFile, true, "Launch with...", false);
}


void Application::List(const os::String &cFile)
{
	std::vector<os::String> cApplications;


    try {
		os::Message message;
		os::String type;
		os::Image *image = NULL;
		m_pcManager->GetTypeAndIcon(cFile, os::Point(24, 24), &type, &image, &message);
		int32 nHandlerCount;

		if (message.FindInt32("handler_count", &nHandlerCount) == 0) {
			os::String handler;
			for (int i = 0; i < nHandlerCount; i++) {
				message.FindString("handler", &handler, i);
				cApplications.push_back(handler);
			}
		}
	}

    catch (...)
    {
    }

	if (cApplications.size() > 0) {
		printf("------------------------------------\n");
		printf("List of Available Applications: \n");
		for (int i=0; i < cApplications.size(); i++) {
			printf("\t%s\n", cApplications[i].c_str());
		}
		printf("------------------------------------\n");
	}
	else{
		printf("No available applications\n");
	}
    //PostMessage(os::M_QUIT);
}


void Application::OpenFile(const os::String &cFile, const os::String &cHandler)
{

    if (cHandler == "")
    {
        try
        {
            os::Message *message = new os::Message();
            os::String type;
            os::String handler;
            os::Image *image = NULL;
            m_pcManager->GetTypeAndIcon(cFile, os::Point(24, 24), &type, &image, message);
            if (message->FindString("default_handler", &handler) == 0)
            {

                struct stat sStat;
                bool bValid = (lstat(handler.c_str(), &sStat) >= 0);
                if (!bValid)
                {
                    return List(cFile);
                }


                else if (fork() == 0)
                {
                    execlp(handler.c_str(), handler.c_str(), cFile.c_str(), NULL);
                }
            }
            else
            {
                return OpenFile(cFile, "/system/bin/aedit");
            }
        }
        catch (...)
        {
        }
    }
    else
    {
        if (fork() == 0)
        {
            execlp(cHandler.c_str(), cHandler.c_str(), cFile.c_str(), NULL);
        }
    }
}

void Application::RevealFile(const os::Path &cPath)
{
    //TODO: This file manager can't highlight the file yet.
    os::String cFile = cPath.GetDir().GetPath();
    if (fork() == 0)
    {
        execlp("/system/bin/filer", "/system/bin/filer", cFile.c_str(), NULL);
    }
}

int main(int argc, char **argv)
{
    Application app = Application(argc, argv);
    return app.Run();
}
