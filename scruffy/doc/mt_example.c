async object MyObject
{
    int a;
};

void MyObject::f ()
{
    this.g ();
}

sync void MyObject::g ()
{
}

int main (void)
{
    MyObject @ref = new MyObject;
}

