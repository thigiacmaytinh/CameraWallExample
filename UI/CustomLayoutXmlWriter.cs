using System;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace WpfCameraWall
{
    public static class CustomLayoutXmlWriter
    {
        public static void SaveOrReplace(string xmlPath, CameraLayout layout)
        {
            if (layout == null)
                return;

            Directory.CreateDirectory(Path.GetDirectoryName(xmlPath)!);

            XDocument doc;

            if (File.Exists(xmlPath))
            {
                doc = XDocument.Load(xmlPath);
                if (doc.Root == null)
                    doc = CreateEmptyDocument();
            }
            else
            {
                doc = CreateEmptyDocument();
            }

            XElement root = doc.Root!;

            // Replace old custom layout with same name.
            XElement? old = root.Elements("ElementConfig")
                .FirstOrDefault(x =>
                    string.Equals(
                        x.Element("Name")?.Value?.Trim(),
                        layout.Name,
                        StringComparison.OrdinalIgnoreCase));

            old?.Remove();

            int nextType = GetNextType(root);

            XElement config = new XElement("ElementConfig",
                new XElement("Name", layout.Name),
                new XElement("Type", nextType),
                new XElement("WndSize", layout.Cells.Count),
                new XElement("RowStretch", BuildStretch(layout.Rows)),
                new XElement("ColumnStretch", BuildStretch(layout.Cols)),
                new XElement("ElementItems",
                    layout.Cells
                        .OrderBy(c => c.CellId)
                        .Select(c =>
                            new XElement("Element",
                                new XAttribute("type", "QWidget"),
                                new XAttribute("id", c.CellId),
                                new XAttribute("row", c.Y),
                                new XAttribute("column", c.X),
                                new XAttribute("rowSpan", c.H),
                                new XAttribute("columnSpan", c.W)
                            )
                        )
                )
            );

            root.Add(config);
            doc.Save(xmlPath);
        }

        private static XDocument CreateEmptyDocument()
        {
            return new XDocument(
                new XElement("Config")
            );
        }

        private static string BuildStretch(int count)
        {
            if (count <= 0)
                return "1";

            return string.Join(",", Enumerable.Repeat("1", count));
        }

        private static int GetNextType(XElement root)
        {
            int maxType = 9999;

            foreach (XElement config in root.Elements("ElementConfig"))
            {
                string? text = config.Element("Type")?.Value;

                if (int.TryParse(text, out int type))
                    maxType = Math.Max(maxType, type);
            }

            return maxType + 1;
        }
    }
}