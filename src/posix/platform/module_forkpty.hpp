class StreamForkpty : public FileDescriptor
{
private:
    int OpenFile(const char *aDevice, Arguments &aArguments);
};

