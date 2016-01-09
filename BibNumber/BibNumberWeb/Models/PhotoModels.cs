using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace BibNumberWeb
{
    public partial class Photo
    {
        private List<String> _bibNumbers { get; set; }

        public List<string> BibNumbers
        {
            get 
            {
                if (_bibNumbers == null)
                {
                    _bibNumbers = new List<string>();
                }
                
                return _bibNumbers; 
            }
            //set { _bibNumbers = value; }
        }

        //[Required]
        public string BibNumbersAsString
        {
            get { return _bibNumbers == null || _bibNumbers.Count == 0 ? "" : _bibNumbers.Aggregate((c, n) => c + "," + n); }
            set { _bibNumbers = value.Split(',').ToList(); }
        }
    }
}