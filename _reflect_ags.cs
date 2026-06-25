using System;
using System.Linq;
using System.Reflection;
using AGS.Types;

static void Dump(Type t)
{
    Console.WriteLine("TYPE " + t.FullName);
    foreach (var p in t.GetProperties(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly).OrderBy(x => x.Name))
        Console.WriteLine("PROP " + p.PropertyType.Name + " " + p.Name);
    foreach (var e in t.GetEvents(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly).OrderBy(x => x.Name))
        Console.WriteLine("EVENT " + e.EventHandlerType.Name + " " + e.Name);
    foreach (var m in t.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly).OrderBy(x => x.Name))
        Console.WriteLine("METHOD " + m.Attributes + " " + m.Name + "(" + string.Join(", ", m.GetParameters().Select(p => p.ParameterType.Name + " " + p.Name)) + ")");
}

Dump(typeof(ContentDocument));
Dump(typeof(EditorContentPanel));
Dump(typeof(IGUIController));
